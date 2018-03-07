/*
* Copyright (c) <2017> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
*/

#include "HoudiniCSV.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "EditorFramework/AssetImportData.h"
#endif

#define LOCTEXT_NAMESPACE HOUDINI_NIAGARA_LOCTEXT_NAMESPACE 

DEFINE_LOG_CATEGORY(LogHoudiniNiagara);
 
UHoudiniCSV::UHoudiniCSV( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer ),
	NumberOfLines( -1 ),
	NumberOfColumns( -1 )
{
        
}

void UHoudiniCSV::SetFileName( const FString& TheFileName )
{
    FileName = TheFileName;
}

bool UHoudiniCSV::UpdateFromFile( const FString& TheFileName )
{
    if ( TheFileName.IsEmpty() )
	return false;

    // Check the CSV file exists
    const FString FullCSVFilename = FPaths::ConvertRelativePathToFull( TheFileName );
    if ( !FPaths::FileExists( FullCSVFilename ) )
	return false;

    FileName = TheFileName;

    // Parse the file to a string array
    TArray<FString> StringArray;
    if ( !FFileHelper::LoadFileToStringArray( StringArray, *FullCSVFilename ) )
	return false;

    return UpdateFromStringArray( StringArray );
}

bool UHoudiniCSV::UpdateFromStringArray( TArray<FString>& RawStringArray )
{
    // Reset the CSV sizes
    NumberOfColumns = 0;
    NumberOfLines = 0;

    // Reset the position, normal and time indexes
    PositionColumnIndex = INDEX_NONE;
    NormalColumnIndex = INDEX_NONE;
    TimeColumnIndex = INDEX_NONE;

    if ( RawStringArray.Num() <= 0 )
    {
	UE_LOG( LogHoudiniNiagara, Error, TEXT( "Could not load the CSV file, error: not enough lines." ) );
	return false;
    }

    // Remove empty lines from the CSV
    RawStringArray.RemoveAll( [&]( const FString& InString ) { return InString.IsEmpty(); } );

    // Number of lines in the CSV (ignoring the title line)
    NumberOfLines = RawStringArray.Num() - 1;
    if ( NumberOfLines < 1 )
    {
	UE_LOG( LogHoudiniNiagara, Error, TEXT( "Could not load the CSV file, error: not enough lines." ) );
	return false;
    }

    // Get the number of values per lines via the title line
    FString TitleString = RawStringArray[ 0 ];
    TitleRowArray.Empty();
    TitleString.ParseIntoArray( TitleRowArray, TEXT(",") );
    NumberOfColumns = TitleRowArray.Num();
    if ( NumberOfColumns < 1 )
    {
	UE_LOG( LogHoudiniNiagara, Error, TEXT( "Could not load the CSV file, error: not enough columns." ) );
	return false;
    }

    // Look for the position, normal and time attributes indexes
    for ( int32 n = 0; n < TitleRowArray.Num(); n++ )
    {
	if ( TitleRowArray[ n ].Equals( TEXT("P"), ESearchCase::IgnoreCase )
	    || TitleRowArray[ n ].Equals( TEXT("Px"), ESearchCase::IgnoreCase ) 
	    || TitleRowArray[ n ].Equals( TEXT("X"), ESearchCase::IgnoreCase ) )
	{
	    if ( PositionColumnIndex == INDEX_NONE )
		PositionColumnIndex = n;
	}
	else if ( TitleRowArray[ n ].Equals( TEXT("N"), ESearchCase::IgnoreCase ) 
	    || TitleRowArray[ n ].Equals( TEXT("Nx"), ESearchCase::IgnoreCase ) )
	{
	    if ( NormalColumnIndex == INDEX_NONE )
		NormalColumnIndex = n;
	}
	else if ( ( TitleRowArray[ n ].Equals( TEXT("T"), ESearchCase::IgnoreCase ) )
	    || ( TitleRowArray[ n ].Contains( TEXT("time"), ESearchCase::IgnoreCase ) ) )
	{
	    if ( TimeColumnIndex == INDEX_NONE )
		TimeColumnIndex = n;
	}
    }

    // Read the first line of the CSV file, and look for packed vectors value (X,Y,Z)
    // We'll have to expand them in the title row to match the parsed data
    FString FirstLine = RawStringArray[ 1 ];
    int32 FoundPackedVectorCharIndex = 0;
    bool HasPackedVectors = false;
    while ( FoundPackedVectorCharIndex != INDEX_NONE )
    {
	// Try to find ( in the line
	FoundPackedVectorCharIndex = FirstLine.Find( TEXT("("), ESearchCase::IgnoreCase, ESearchDir::FromStart, FoundPackedVectorCharIndex );
	if ( FoundPackedVectorCharIndex == INDEX_NONE )
	    break;
	// We want to know which column this char belong to
	int32 FoundPackedVectorColumnIndex = INDEX_NONE;
	{
	    // Chop the first line up to the found character
	    FString FirstLineLeft = FirstLine.Left( FoundPackedVectorCharIndex );

	    // ReplaceInLine returns the number of occurences of ",", that's what we want! 
	    FoundPackedVectorColumnIndex = FirstLineLeft.ReplaceInline(TEXT(","), TEXT(""));
	}

	if ( !TitleRowArray.IsValidIndex( FoundPackedVectorColumnIndex ) )
	{
	    UE_LOG( LogHoudiniNiagara, Warning,
		TEXT( "Error while parsing the CSV File. Couldn't unpack vector found at character %d in the first line!" ),
		FoundPackedVectorCharIndex + 1 );
	    continue;
	}
	    

	// We found a packed vector, get its sizw
	int32 FoundVectorSize = 0;
	{
	    // Extract the vector string
	    int32 FoundPackedVectorEndCharIndex = FirstLine.Find( TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromStart, FoundPackedVectorCharIndex );
	    FString VectorString = FirstLine.Mid( FoundPackedVectorCharIndex + 1, FoundPackedVectorEndCharIndex - FoundPackedVectorCharIndex - 1 );

	    // Use ReplaceInLine to count the number of , to get the vector's size!
	    FoundVectorSize = VectorString.ReplaceInline( TEXT(","), TEXT("") ) + 1;
	}

	if ( FoundVectorSize < 2 )
	    continue;

	// Increment the number of columns
	NumberOfColumns += ( FoundVectorSize - 1 );

	// Expand TitleRowArray
	if ( ( FoundPackedVectorColumnIndex == PositionColumnIndex ) && ( FoundVectorSize == 3 ) )
	{
	    // Expand P to PX,Py,Pz
	    TitleRowArray[ PositionColumnIndex ] = TEXT("Px");
	    TitleRowArray.Insert( TEXT( "Py" ), PositionColumnIndex + 1 );
	    TitleRowArray.Insert( TEXT( "Pz" ), PositionColumnIndex + 2 );
	}
	else if ( ( FoundPackedVectorColumnIndex == NormalColumnIndex ) && ( FoundVectorSize == 3 ) )
	{
	    // Expand N to Nx,Ny,Nz
	    TitleRowArray[ NormalColumnIndex ] = TEXT("Nx");
	    TitleRowArray.Insert( TEXT("Ny"), NormalColumnIndex + 1 );
	    TitleRowArray.Insert( TEXT("Nz"), NormalColumnIndex + 2 );
	}
	else
	{
	    // Expand the vector's title
	    FString FoundPackedVectortTitle = TitleRowArray[ FoundPackedVectorColumnIndex ];
	    for ( int32 n = 1; n < FoundVectorSize; n++ )
	    {
		FString CurrentTitle = FoundPackedVectortTitle + FString::FromInt( n );
		TitleRowArray.Insert( CurrentTitle, FoundPackedVectorColumnIndex + n );
	    }
	}

	// Eventually offset the stored index
	if ( PositionColumnIndex != INDEX_NONE && ( PositionColumnIndex > FoundPackedVectorColumnIndex) )
	    PositionColumnIndex += FoundVectorSize - 1;

	if ( NormalColumnIndex != INDEX_NONE && ( NormalColumnIndex > FoundPackedVectorColumnIndex) )
	    NormalColumnIndex += FoundVectorSize - 1;

	if ( TimeColumnIndex != INDEX_NONE && ( TimeColumnIndex > FoundPackedVectorColumnIndex) )
	    TimeColumnIndex += FoundVectorSize - 1;

	HasPackedVectors = true;
	FoundPackedVectorCharIndex++;
    }

    // For sanity, Check that the number of columns matches the title row and the first line
    {
	// Check the title row
	if ( NumberOfColumns != TitleRowArray.Num() )
	    UE_LOG( LogHoudiniNiagara, Error,
		TEXT( "Error while parsing the CSV File. Found %d columns but the Title string has %d values! Some values will have an offset!" ),
		NumberOfColumns, TitleRowArray.Num() );

	// Use ReplaceInLine to count the number of columns in the first line and make sure it's correct
	int32 FirstLineColumns = FirstLine.ReplaceInline( TEXT(","), TEXT("") ) + 1;
	if ( NumberOfColumns != FirstLineColumns )
	    UE_LOG( LogHoudiniNiagara, Error,
		TEXT("Error while parsing the CSV File. Found %d columns but found %d values in the first line! Some values will have an offset!" ),
		NumberOfColumns, FirstLineColumns );
    }
    
    // Remove the title row now that it's been processed
    RawStringArray.RemoveAt( 0 );

    // Initialize our different buffers
    FloatCSVData.Empty();
    FloatCSVData.SetNumZeroed( NumberOfLines * NumberOfColumns );
    
    StringCSVData.Empty();
    StringCSVData.SetNumZeroed( NumberOfLines * NumberOfColumns );

     // Extract all the values from the table to the float & string buffers
    TArray<FString> CurrentParsedLine;
    for ( int rowIdx = 0; rowIdx < NumberOfLines; rowIdx++ )
    {
	FString CurrentLine = RawStringArray[ rowIdx ];
	if ( HasPackedVectors )
	{
	    // Clean up the packing characters: ()" from the line so it can be parsed properly
	    CurrentLine.ReplaceInline( TEXT("("), TEXT("") );
	    CurrentLine.ReplaceInline( TEXT(")"), TEXT("") );
	    CurrentLine.ReplaceInline( TEXT("\""), TEXT("") );
	}

	// Parse the current line to an array
	CurrentParsedLine.Empty();
	CurrentLine.ParseIntoArray( CurrentParsedLine, TEXT(",") );

	// Check the parsed line and number of columns match
	if ( NumberOfColumns != CurrentParsedLine.Num() )
	    UE_LOG( LogHoudiniNiagara, Warning,
		TEXT("Error while parsing the CSV File. Line %d has %d values instead of the expected %d!"),
		rowIdx + 1, CurrentParsedLine.Num(), NumberOfColumns );

	// Store the CSV Data in the buffer
	// The data is stored transposed in those buffers
	for ( int colIdx = 0; colIdx < NumberOfColumns; colIdx++ )
	{
	    // Get the string value for the current column
	    FString CurrentVal = TEXT("0");
	    if ( CurrentParsedLine.IsValidIndex( colIdx ) )
		CurrentVal = CurrentParsedLine[ colIdx ];
	    else
	    {
		UE_LOG( LogHoudiniNiagara, Warning,
		    TEXT("Error while parsing the CSV File. Line %d has an invalid value for column %d!"),
		    rowIdx + 1, colIdx + 1 );
	    }

	    // Convert the string value to a float in the buffer
	    FloatCSVData[ rowIdx + ( colIdx * NumberOfLines ) ] = FCString::Atof( *CurrentVal );

	    // Keep the original string value in a buffer too
	    StringCSVData[ rowIdx + ( colIdx * NumberOfLines ) ] = CurrentVal;
	}	
    }

    return true;
}


// Returns the float value at a given point in the CSV file
bool UHoudiniCSV::GetCSVFloatValue( const int32& lineIndex, const int32& colIndex, float& value )
{
    if ( lineIndex < 0 || lineIndex >= NumberOfLines )
	return false;

    if ( colIndex < 0 || colIndex >= NumberOfColumns )
	return false;

    int32 Index = lineIndex + ( colIndex * NumberOfLines );
    if ( FloatCSVData.IsValidIndex( Index ) )
    {
	value = FloatCSVData[ Index ];
	return true;
    }

    return false;
}

// Returns the float value at a given point in the CSV file
bool UHoudiniCSV::GetCSVStringValue( const int32& lineIndex, const int32& colIndex, FString& value )
{
    if ( lineIndex < 0 || lineIndex >= NumberOfLines )
	return false;

    if ( colIndex < 0 || colIndex >= NumberOfColumns )
	return false;

    int32 Index = lineIndex + ( colIndex * NumberOfLines );
    if ( StringCSVData.IsValidIndex( Index ) )
    {
	value = StringCSVData[ Index ];
	return true;
    }

    return false;
}

// Returns a Vector 3 for a given point in the CSV file
bool UHoudiniCSV::GetCSVVectorValue( const int32& lineIndex, const int32& colIndex, FVector& value, const bool& DoSwap, const bool& DoScale )
{
    FVector V = FVector::ZeroVector;
    if ( !GetCSVFloatValue( lineIndex, colIndex, V.X ) )
	return false;

    if ( !GetCSVFloatValue( lineIndex, colIndex + 1, V.Y ) )
	return false;

    if ( !GetCSVFloatValue( lineIndex, colIndex + 2, V.Z ) )
	return false;

    if ( DoScale )
	V *= 100.0f;

    value = V;

    if ( DoSwap )
    {
	value.Y = V.Z;
	value.Z = V.Y;
    }

    return true;
}

// Returns a Vector 3 for a given point in the CSV file
bool UHoudiniCSV::GetCSVPositionValue( const int32& lineIndex, FVector& value )
{
    FVector V = FVector::ZeroVector;
    if ( !GetCSVVectorValue( lineIndex, PositionColumnIndex, V, true, true ) )
	return false;

    value = V;

    return true;
}

// Returns a Vector 3 for a given point in the CSV file
bool UHoudiniCSV::GetCSVNormalValue( const int32& lineIndex, FVector& value )
{
    FVector V = FVector::ZeroVector;
    if ( !GetCSVVectorValue(lineIndex, NormalColumnIndex, V, true, false ) )
	return false;

    value = V;

    return true;
}

// Returns a time value for a given point in the CSV file
bool UHoudiniCSV::GetCSVTimeValue( const int32& lineIndex, float& value )
{
    float temp;
    if ( !GetCSVFloatValue( lineIndex, TimeColumnIndex, temp ) )
	return false;

    value = temp;

    return true;
}

// Returns the number of points found in the CSV file
int32 UHoudiniCSV::GetNumberOfPointsInCSV()
{
    return NumberOfLines;
}

// Get the last index of the particles with a time value smaller or equal to desiredTime
// If the CSV file doesn't have time informations, returns false and set the LastIndex to the last particle
// If desiredTime is smaller than the first particle, LastIndex will be set to -1
// If desiredTime is higher than the last particle in the csv file, LastIndex will be set to the last particle's index
bool UHoudiniCSV::GetLastParticleIndexAtTime( const float& desiredTime, int32& lastIndex )
{
    // If we dont have proper time info, always return the last line
    if ( TimeColumnIndex < 0 || TimeColumnIndex >= NumberOfColumns )
    {
	lastIndex = NumberOfLines - 1;
	return false;
    }

    float temp_time = 0.0f;
    // Check first if we have anything to spawn at the current time by looking at the last value
    if ( GetCSVTimeValue( NumberOfLines - 1, temp_time ) && temp_time < desiredTime )
    {
	// We didn't find a suitable index because the desired time is higher than our last time value
	lastIndex = NumberOfLines - 1;
	return true;
    }	

    // Iterates through all the particles
    lastIndex = INDEX_NONE;
    for ( int32 n = 0; n < NumberOfLines; n++ )
    {
	if ( GetCSVTimeValue( n, temp_time ) )
	{
	    if ( temp_time == desiredTime )
	    {
		lastIndex = n;
	    }
	    else if ( temp_time > desiredTime )
	    {
		lastIndex = n - 1;
		return true;
	    }
	}
    }

    // We didn't find a suitable index because the desired time is higher than our last time value
    if ( lastIndex == INDEX_NONE )
	lastIndex = NumberOfLines - 1;

    return true;
}

// Returns the column index for a given string
bool UHoudiniCSV::GetColumnIndexFromString( const FString& ColumnTitle, int32& ColumnIndex )
{
    if ( !TitleRowArray.Find( ColumnTitle, ColumnIndex ) )
	return true;

    // Handle packed positions/normals here
    if ( ColumnTitle.Equals( "P" ) )
	return TitleRowArray.Find( TEXT( "Px" ), ColumnIndex );
    else if ( ColumnTitle.Equals( "N" ) )
	return TitleRowArray.Find( TEXT( "Nx" ), ColumnIndex );

    return false;
}

// Returns the float value at a given point in the CSV file
bool UHoudiniCSV::GetCSVFloatValue( const int32& lineIndex, const FString& ColumnTitle, float& value )
{
    int32 ColIndex = -1;
    if ( !GetColumnIndexFromString( ColumnTitle, ColIndex ) )
	return false;

    return GetCSVFloatValue( lineIndex, ColIndex, value );
}


// Returns the string value at a given point in the CSV file
bool UHoudiniCSV::GetCSVStringValue( const int32& lineIndex, const FString& ColumnTitle, FString& value )
{
    int32 ColIndex = -1;
    if ( !GetColumnIndexFromString( ColumnTitle, ColIndex ) )
	return false;

    return GetCSVStringValue( lineIndex, ColIndex, value );
}

#if WITH_EDITORONLY_DATA
void UHoudiniCSV::PostInitProperties()
{
    if ( !HasAnyFlags( RF_ClassDefaultObject ) )
    {
	AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
    }

    Super::PostInitProperties();
}
#endif

#undef LOCTEXT_NAMESPACE
