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
 
UHoudiniCSV::UHoudiniCSV( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer ),
	NumberOfLines(-1),
	NumberOfColumns(-1)
{
        
}

void UHoudiniCSV::SetFileName(const FString& TheFileName)
{
    FileName = TheFileName;
}

bool UHoudiniCSV::UpdateFromFile(const FString& TheFileName)
{
    if (TheFileName.IsEmpty() )
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

    // Reset the position Indexes
    PositionColumnIndex = -1;
    NormalColumnIndex = -1;
    TimeColumnIndex = -1;

    if ( RawStringArray.Num() <= 0 )
	return false;

    // Remove empty lines from the CSV
    RawStringArray.RemoveAll( [&]( const FString& InString ) { return InString.IsEmpty(); } );

    // Number of lines in the CSV (ignoring the title line)
    NumberOfLines = RawStringArray.Num() - 1;
    if ( NumberOfLines < 1 )
	return false;

    // Get the number of values per lines via the title line
    FString TitleString = RawStringArray[ 0 ];
    TitleRowArray.Empty();
    TitleString.ParseIntoArray( TitleRowArray, TEXT(",") );
    if ( TitleRowArray.Num() <= 0 )
	return false;

    NumberOfColumns = TitleRowArray.Num();
    if ( NumberOfColumns < 1 )
	return false;

    // Look for packed attributes in the title row
    // Also find the position, normal and time attribute positions
    bool HasPackedPositions = false;    
    bool HasPackedNormals = false;    

    for ( int32 n = 0; n < TitleRowArray.Num(); n++ )
    {
	if ( TitleRowArray[n].Equals(TEXT("P"), ESearchCase::IgnoreCase) )
	{
	    HasPackedPositions = true;
	    NumberOfColumns += 2;
	    PositionColumnIndex = n;
	}
	else if ( TitleRowArray[n].Equals(TEXT("N"), ESearchCase::IgnoreCase) )
	{
	    HasPackedNormals = true;
	    NumberOfColumns += 2;
	    NormalColumnIndex = n;
	}
	else if ( TitleRowArray[n].Equals(TEXT("Px"), ESearchCase::IgnoreCase) || TitleRowArray[n].Equals(TEXT("X"), ESearchCase::IgnoreCase))
	{
	    HasPackedPositions = false;
	    PositionColumnIndex = n;
	}
	else if ( TitleRowArray[n].Equals(TEXT("Nx"), ESearchCase::IgnoreCase) )
	{
	    HasPackedNormals = false;
	    NormalColumnIndex = n;
	}
	else if ( ( TitleRowArray[n].Equals(TEXT("T"), ESearchCase::IgnoreCase) )
	    || ( TitleRowArray[n].Contains(TEXT("time"), ESearchCase::IgnoreCase) ) )
	{
	    TimeColumnIndex = n;
	}
    }

    // If we have packed positions or normals,
    // we might have to offset other indexes and update the title line too
    if ( HasPackedPositions )
    {
	TitleRowArray[ PositionColumnIndex ] = TEXT("Px");
	TitleRowArray.Insert( TEXT("Py"), PositionColumnIndex + 1 );
	TitleRowArray.Insert( TEXT("Pz"), PositionColumnIndex + 2 );

	if ( NormalColumnIndex > PositionColumnIndex )  
	    NormalColumnIndex += 2;

	if ( TimeColumnIndex > PositionColumnIndex )
	    TimeColumnIndex += 2;
    }

    if ( HasPackedNormals )
    {
	TitleRowArray[ NormalColumnIndex ] = TEXT("Nx");
	TitleRowArray.Insert( TEXT("Ny"), NormalColumnIndex + 1 );
	TitleRowArray.Insert( TEXT("Nz"), NormalColumnIndex + 2 );

	if ( PositionColumnIndex > NormalColumnIndex )
	    PositionColumnIndex += 2;

	if ( TimeColumnIndex > NormalColumnIndex )
	    TimeColumnIndex += 2;
    }

    // Remove the title row now that it's been processed
    RawStringArray.RemoveAt(0);

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
	if ( HasPackedNormals || HasPackedPositions )
	{
	    // Clean up the packing "( and )" from the line so it can be parsed properly
	    CurrentLine.ReplaceInline(TEXT("\"("), TEXT(""));
	    CurrentLine.ReplaceInline(TEXT(")\""), TEXT(""));
	}

	// Parse the current line to an array
	CurrentParsedLine.Empty();
	CurrentLine.ParseIntoArray( CurrentParsedLine, TEXT(",") );

	// Store the CSV Data in the buffer
	// The data is stored transposed in those buffers
	for (int colIdx = 0; colIdx < NumberOfColumns; colIdx++)
	{
	    // Convert the string value to a float in the buffer
	    FString CurrentVal = CurrentParsedLine[ colIdx ];
	    FloatCSVData[ rowIdx + ( colIdx * NumberOfLines ) ] = FCString::Atof( *CurrentVal );

	    // Keep the original string value in a buffer too
	    StringCSVData[ rowIdx + ( colIdx * NumberOfLines ) ] = CurrentVal;
	}	
    }

    return true;
}


// Returns the float value at a given point in the CSV file
bool UHoudiniCSV::GetCSVFloatValue(const int32& lineIndex, const int32& colIndex, float& value )
{
    if ( lineIndex < 0 || lineIndex >= NumberOfLines )
	return false;

    if ( colIndex < 0 || colIndex >= NumberOfColumns )
	return false;

    int32 Index = lineIndex + (colIndex * NumberOfLines);
    if ( FloatCSVData.IsValidIndex(Index) )
    {
	value = FloatCSVData[ Index ];
	return true;
    }

    return false;
}

// Returns the float value at a given point in the CSV file
bool UHoudiniCSV::GetCSVStringValue(const int32& lineIndex, const int32& colIndex, FString& value)
{
    if ( lineIndex < 0 || lineIndex >= NumberOfLines )
	return false;

    if ( colIndex < 0 || colIndex >= NumberOfColumns )
	return false;

    int32 Index = lineIndex + (colIndex * NumberOfLines );
    if ( StringCSVData.IsValidIndex( Index ) )
    {
	value = StringCSVData[ Index ];
	return true;
    }

    return false;
}

// Returns a Vector 3 for a given point in the CSV file
bool UHoudiniCSV::GetCSVVectorValue(const int32& lineIndex, const int32& colIndex, FVector& value, const bool& DoSwap, const bool& DoScale)
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
bool UHoudiniCSV::GetCSVNormalValue(const int32& lineIndex, FVector& value)
{
    FVector V = FVector::ZeroVector;
    if ( !GetCSVVectorValue(lineIndex, NormalColumnIndex, V, true, false ) )
	return false;

    value = V;

    return true;
}

// Returns a time value for a given point in the CSV file
bool UHoudiniCSV::GetCSVTimeValue(const int32& lineIndex, float& value)
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

// Get the last index of the particles to be spawned at time t
// Invalid Index are used to indicate edge cases:
// -1 will be returned if no particles have been spawned (t is smaller than the first particle time)
// NumberOfLines will be returned if all particles in the CSV have been spawned ( t is higher than the last particle time)
bool UHoudiniCSV::GetLastParticleIndexAtTime( const float& desiredTime, int32& lastIndex )
{
    // If we dont have time info, always return the last line
    if ( TimeColumnIndex < 0 || TimeColumnIndex >= NumberOfColumns )
    {
	lastIndex = NumberOfLines - 1;
	return false;
    }

    float temp_time = 0.0f;
    
    // Check first if we have anything to spawn at the current time by looking at the last value
    if ( GetCSVTimeValue( NumberOfLines - 1, temp_time ) && temp_time < desiredTime )
    {
	lastIndex = NumberOfLines;
	return true;
    }	

    // Iterates through all the particles
    for ( int32 n = 0; n < NumberOfLines; n++ )
    {
	if ( GetCSVTimeValue( n, temp_time ) )
	{
	    if ( temp_time == desiredTime )
	    {
		lastIndex = n;
		return true;
	    }
	    else if ( temp_time > desiredTime )
	    {
		lastIndex = n - 1;
		return true;
	    }
	}
    }

    lastIndex = NumberOfLines;
    return true;
}

// Returns the column index for a given string
bool UHoudiniCSV::GetColumnIndex(const FString& ColumnTitle, int32& ColumnIndex )
{
    return TitleRowArray.Find(ColumnTitle, ColumnIndex);
}

// Returns the float value at a given point in the CSV file
bool UHoudiniCSV::GetCSVFloatValue( const int32& lineIndex, const FString& ColumnTitle, float& value )
{
    int32 ColIndex = -1;
    if ( !GetColumnIndex(ColumnTitle, ColIndex ) )
	return false;

    return GetCSVFloatValue(lineIndex, ColIndex, value );
}


// Returns the string value at a given point in the CSV file
bool UHoudiniCSV::GetCSVStringValue(const int32& lineIndex, const FString& ColumnTitle, FString& value)
{
    int32 ColIndex = -1;
    if ( !GetColumnIndex(ColumnTitle, ColIndex ) )
	return false;

    return GetCSVStringValue( lineIndex, ColIndex, value );
}
