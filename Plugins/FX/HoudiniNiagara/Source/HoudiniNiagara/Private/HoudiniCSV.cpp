/*
* Copyright (c) <2018> Side Effects Software Inc.
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
#include "Math/NumericLimits.h"

#if WITH_EDITOR
#include "EditorFramework/AssetImportData.h"
#endif

#define LOCTEXT_NAMESPACE "HoudiniNiagaraCSVAsset"

DEFINE_LOG_CATEGORY(LogHoudiniNiagara);

struct FHoudiniCSVSortPredicate
{
	FHoudiniCSVSortPredicate(const int32 &InTimeCol, const int32 &InIDCol )
		: TimeColumnIndex( InTimeCol ), IDColumnIndex( InIDCol )
	{}

	bool operator()( const TArray<FString>& A, const TArray<FString>& B ) const
	{
		float ATime = TNumericLimits< float >::Lowest();
		if ( A.IsValidIndex( TimeColumnIndex ) )
			ATime = FCString::Atof( *A[ TimeColumnIndex ] );

		float BTime = TNumericLimits< float >::Lowest();
		if ( B.IsValidIndex( TimeColumnIndex ) )
			BTime = FCString::Atof( *B[ TimeColumnIndex ] );

		if ( ATime != BTime )
		{
			return ATime < BTime;
		}
		else
		{
			float AID = TNumericLimits< float >::Lowest();
			if ( A.IsValidIndex( IDColumnIndex ) )
				AID = FCString::Atof( *A[ IDColumnIndex ]);

			float BID = TNumericLimits< float >::Lowest();
			if ( B.IsValidIndex( IDColumnIndex ) )
				BID = FCString::Atof( *B[ IDColumnIndex ] );

			return AID <= BID;
		}
	}

	int32 TimeColumnIndex;
	int32 IDColumnIndex;
};
 
UHoudiniCSV::UHoudiniCSV( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer ),
	NumberOfRows( -1 ),
	NumberOfColumns( -1 ),
	NumberOfPoints( -1 )
{
	SpecialAttributesColumnIndexes.Init(INDEX_NONE, EHoudiniAttributes::HOUDINI_ATTR_SIZE);

	UseCustomTitleRow = false;
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
    NumberOfRows = 0;
	NumberOfPoints = 0;

	// Reset the column indexes of the special attributes
	SpecialAttributesColumnIndexes.Init( INDEX_NONE, EHoudiniAttributes::HOUDINI_ATTR_SIZE );

    if ( RawStringArray.Num() <= 0 )
    {
		UE_LOG( LogHoudiniNiagara, Error, TEXT( "Could not load the CSV file, error: not enough rows in the file." ) );
		return false;
    }

    // Remove empty rows from the CSV
    RawStringArray.RemoveAll( [&]( const FString& InString ) { return InString.IsEmpty(); } );

    // Number of rows in the CSV (ignoring the title row)
    NumberOfRows = RawStringArray.Num() - 1;
    if ( NumberOfRows < 1 )
    {
		UE_LOG( LogHoudiniNiagara, Error, TEXT( "Could not load the CSV file, error: not enough rows in the file." ) );
		return false;
    }

	// See if we need to use a custom title row
	// The custom title row will be ignored if it is empty or only composed of spaces
	FString TitleRow = SourceTitleRow;
	TitleRow.ReplaceInline(TEXT(" "), TEXT(""));
	if ( TitleRow.IsEmpty() )
		UseCustomTitleRow = false;

	if ( !UseCustomTitleRow )
		SourceTitleRow = RawStringArray[0];

	// Parses the CSV file's title row to update the column indexes of special values we're interested in
	// Also look for packed vectors in the first row and update the indexes accordingly
	bool HasPackedVectors = false;
	if ( !ParseCSVTitleRow( SourceTitleRow, RawStringArray[1], HasPackedVectors ) )
		return false;
    
    // Remove the title row now that it's been processed
    RawStringArray.RemoveAt( 0 );

	// Parses each string of the csv file to a string array
	TArray< TArray< FString > > ParsedStringArrays;
	ParsedStringArrays.SetNum( NumberOfRows );
	for ( int32 rowIdx = 0; rowIdx < NumberOfRows; rowIdx++ )
	{
		// Get the current row
		FString CurrentRow = RawStringArray[ rowIdx ];
		if ( HasPackedVectors )
		{
			// Clean up the packing characters: ()" from the row so it can be parsed properly
			CurrentRow.ReplaceInline( TEXT("("), TEXT("") );
			CurrentRow.ReplaceInline( TEXT(")"), TEXT("") );
			CurrentRow.ReplaceInline( TEXT("\""), TEXT("") );
		}

		// Parse the current row to an array
		TArray<FString> CurrentParsedRow;
		CurrentRow.ParseIntoArray( CurrentParsedRow, TEXT(",") );

		// Check that the parsed row and number of columns match
		if ( NumberOfColumns != CurrentParsedRow.Num() )
			UE_LOG( LogHoudiniNiagara, Warning,
			TEXT("Error while parsing the CSV File. Row %d has %d values instead of the expected %d!"),
			rowIdx + 1, CurrentParsedRow.Num(), NumberOfColumns );

		// Store the parsed row
		ParsedStringArrays[ rowIdx ] = CurrentParsedRow;
	}

	// If we have time values, we have to make sure the csv rows are sorted by time
	int32 TimeColumnIndex = GetAttributeColumnIndex(EHoudiniAttributes::TIME);
	int32 IDColumnIndex = GetAttributeColumnIndex(EHoudiniAttributes::POINTID);
	if ( TimeColumnIndex != INDEX_NONE )
	{
		// First check if we need to sort the array
		bool NeedToSort = false;
		float PreviousTimeValue = 0.0f;
		for ( int32 rowIdx = 0; rowIdx < ParsedStringArrays.Num(); rowIdx++ )
		{
			if ( !ParsedStringArrays[ rowIdx ].IsValidIndex( TimeColumnIndex ) )
				continue;

			// Get the current time value
			float CurrentValue = FCString::Atof(*(ParsedStringArrays[ rowIdx ][ TimeColumnIndex ]));
			if ( rowIdx == 0 )
			{
				PreviousTimeValue = CurrentValue;
				continue;
			}

			// Time values arent sorted properly
			if ( PreviousTimeValue > CurrentValue )
			{
				NeedToSort = true;
				break;
			}
		}

		if ( NeedToSort )
		{
			// We need to sort the CSV rows by their time values
			ParsedStringArrays.Sort<FHoudiniCSVSortPredicate>( FHoudiniCSVSortPredicate( TimeColumnIndex, IDColumnIndex ) );
		}
	}

    // Initialize our different buffers
    FloatCSVData.Empty();
    FloatCSVData.SetNumZeroed( NumberOfRows * NumberOfColumns );

	/*
    StringCSVData.Empty();
    StringCSVData.SetNumZeroed( NumberOfRows * NumberOfColumns );
	*/
	// Due to the way that some of the DI functions work,
	// we expect that the point IDs start at zero, and increment as the points are spawned
	// Make sure this is the case by converting the point IDs as we read them
	int32 NextPointID = 0;
	TMap<float, int32> HoudiniIDToNiagaraIDMap;

	// We also keep track of the row indexes for each time values
	//float lastTimeValue = 0.0;
	//TimeValuesIndexes.Empty();

	// And the row indexes for each point
	PointValueIndexes.Empty();

    // Extract all the values from the table to the float & string buffers
    TArray<FString> CurrentParsedRow;
    for ( int rowIdx = 0; rowIdx < ParsedStringArrays.Num(); rowIdx++ )
    {
		CurrentParsedRow = ParsedStringArrays[ rowIdx ];

		// Store the CSV Data in the buffers
		// The data is stored transposed in those buffers
		int32 CurrentID = -1;
		for ( int colIdx = 0; colIdx < NumberOfColumns; colIdx++ )
		{
			// Get the string value for the current column
			FString CurrentVal = TEXT("0");
			if ( CurrentParsedRow.IsValidIndex( colIdx ) )
			{
				CurrentVal = CurrentParsedRow[ colIdx ];
			}
			else
			{
				UE_LOG( LogHoudiniNiagara, Warning,
				TEXT("Error while parsing the CSV File. Row %d has an invalid value for column %d!"),
				rowIdx + 1, colIdx + 1 );
			}

			// Convert the string value to a float
			float FloatValue = FCString::Atof( *CurrentVal );

			// Handle point IDs here
			if ( colIdx == IDColumnIndex )
			{
				// The point ID may need to be replaced
				if ( !HoudiniIDToNiagaraIDMap.Contains( FloatValue ) )
				{
					// We found a new point, so we add it to the ID map
					HoudiniIDToNiagaraIDMap.Add( FloatValue, NextPointID++ );

					// Add a new array for that point's indexes
					PointValueIndexes.Add( FPointIndexes() );
				}

				// Get the Niagara ID from this Houdini ID
				CurrentID = HoudiniIDToNiagaraIDMap[ FloatValue ];
				FloatValue = (float)CurrentID;

				// Add the current row to this point's row index list
				PointValueIndexes[ CurrentID ].RowIndexes.Add( rowIdx );
			}

			/*
			if ( colIdx == TimeColumnIndex )
			{
				// Keep track of new time values row indexes
				if ( ( FloatValue != lastTimeValue ) || ( rowIdx == 0 ) )
				{
					TimeValuesIndexes.Add( FloatValue, rowIdx );
					lastTimeValue = FloatValue;
				}
			}
			*/

			// Store the Value in the buffer
			FloatCSVData[ rowIdx + ( colIdx * NumberOfRows ) ] = FloatValue;

			/*
			// Keep the original string value in a buffer too
			StringCSVData[ rowIdx + ( colIdx * NumberOfRows ) ] = CurrentVal;
			*/
		}

		// If we dont have Point ID informations, we still want to fill the PointValueIndexes array
		if ( !IsValidAttributeColumnIndex( EHoudiniAttributes::POINTID ) )
		{
			// Each row is considered its own point
			PointValueIndexes.Add(FPointIndexes());
			PointValueIndexes[ rowIdx ].RowIndexes.Add( rowIdx );
		}
    }
	
	NumberOfPoints = HoudiniIDToNiagaraIDMap.Num();
	if ( NumberOfPoints <= 0 )
		NumberOfPoints = NumberOfRows;

	// Look for point specific attributes to build some helper arrays
	int32 LifeColumnIndex = GetAttributeColumnIndex(EHoudiniAttributes::LIFE);
	int32 AliveColumnIndex = GetAttributeColumnIndex(EHoudiniAttributes::ALIVE);
	int32 TypeColumnIndex = GetAttributeColumnIndex(EHoudiniAttributes::TYPE);

	SpawnTimes.Empty();
	SpawnTimes.Init( -1.0f, NumberOfPoints );

	LifeValues.Empty();
	LifeValues.Init( -1.0f,  NumberOfPoints );

	PointTypes.Empty();
	PointTypes.Init( -1, NumberOfPoints );
	
	float MaxTime = -1.0f;
	for ( int rowIdx = 0; rowIdx < NumberOfRows; rowIdx++ )
	{
		// Get the point ID
		int32 CurrentID = rowIdx;
		if ( IDColumnIndex != INDEX_NONE )
			CurrentID = (int32)FloatCSVData[ rowIdx + ( IDColumnIndex * NumberOfRows ) ];

		// Get the time value for the current row
		float CurrentTime = 0.0f;
		if ( TimeColumnIndex != INDEX_NONE )
			CurrentTime = FloatCSVData[ rowIdx + ( TimeColumnIndex * NumberOfRows ) ];

		if ( LifeColumnIndex != INDEX_NONE )
		{	
			if ( SpawnTimes[ CurrentID ] < 0.0f )
			{
				// Set spawn time and life using life values
				float CurrentLife = FloatCSVData [rowIdx + (LifeColumnIndex * NumberOfRows) ];
				SpawnTimes[ CurrentID ] = CurrentTime;
				LifeValues[ CurrentID ] = CurrentLife;
			}
		}
		else if ( AliveColumnIndex != INDEX_NONE )
		{
			// Set spawn time and life using the alive bool values
			bool CurrentAlive = ( FloatCSVData[ rowIdx + ( AliveColumnIndex * NumberOfRows ) ] == 1.0f );
			if ( ( SpawnTimes[ CurrentID ] < 0.0f ) && CurrentAlive )
			{
				// Spawn time is when the point is first seen alive
				SpawnTimes[ CurrentID ] = CurrentTime;
			}
			else if ( ( SpawnTimes[ CurrentID ] >= 0.0f ) && !CurrentAlive )
			{
				// Life is the difference between spawn time and time of death
				LifeValues[ CurrentID ] = CurrentTime - SpawnTimes[ CurrentID ];
			}
		}
		else
		{
			// No life or alive value, spawn time is the first time we have a value for this point
			if ( SpawnTimes [ CurrentID ] == INDEX_NONE )
				SpawnTimes[ CurrentID ] = CurrentTime;
		}

		// Keep track of the point type at spawn
		if ( PointTypes[ CurrentID ] < 0 )
		{
			float CurrentType = 0.0f;
			if (TypeColumnIndex != INDEX_NONE)
				GetFloatValue( rowIdx, TypeColumnIndex, CurrentType );

			PointTypes[ CurrentID ] = (int32)CurrentType;
		}

	}
    return true;
}

bool UHoudiniCSV::ParseCSVTitleRow( const FString& TitleRow, const FString& FirstValueRow, bool& HasPackedVectors )
{
	// Get the number of values per row via the title row
    ColumnTitleArray.Empty();
    TitleRow.ParseIntoArray( ColumnTitleArray, TEXT(",") );
    NumberOfColumns = ColumnTitleArray.Num();
    if ( NumberOfColumns < 1 )
    {
		UE_LOG( LogHoudiniNiagara, Error, TEXT( "Could not load the CSV file, error: not enough columns." ) );
		return false;
    }

    // Look for the position, normal and time attributes indexes
    for ( int32 n = 0; n < ColumnTitleArray.Num(); n++ )
    {
		// Remove spaces from the title row
		ColumnTitleArray[ n ].ReplaceInline( TEXT(" "), TEXT("") );

		FString CurrentTitle = ColumnTitleArray[ n ];
		if ( CurrentTitle.Equals( TEXT("P"), ESearchCase::IgnoreCase )
			|| CurrentTitle.Equals( TEXT("Px"), ESearchCase::IgnoreCase )
			|| CurrentTitle.Equals( TEXT("X"), ESearchCase::IgnoreCase )
			|| CurrentTitle.Equals(TEXT("pos"), ESearchCase::IgnoreCase ) )
		{
			if ( !IsValidAttributeColumnIndex(EHoudiniAttributes::POSITION))
				SpecialAttributesColumnIndexes[EHoudiniAttributes::POSITION] = n;
		}
		else if ( CurrentTitle.Equals( TEXT("N"), ESearchCase::IgnoreCase )
			|| CurrentTitle.Equals( TEXT("Nx"), ESearchCase::IgnoreCase ) )
		{
			if (!IsValidAttributeColumnIndex(EHoudiniAttributes::NORMAL))
				SpecialAttributesColumnIndexes[EHoudiniAttributes::NORMAL] = n;
		}
		else if ( ( CurrentTitle.Equals( TEXT("T"), ESearchCase::IgnoreCase ) )
			|| ( CurrentTitle.Contains( TEXT("time"), ESearchCase::IgnoreCase ) ) )
		{
			if (!IsValidAttributeColumnIndex(EHoudiniAttributes::TIME))
				SpecialAttributesColumnIndexes[EHoudiniAttributes::TIME] = n;
		}
		else if ( ( CurrentTitle.Equals( TEXT("#"), ESearchCase::IgnoreCase ) )
			|| ( CurrentTitle.Equals( TEXT("id"), ESearchCase::IgnoreCase ) ) )
		{
			if (!IsValidAttributeColumnIndex(EHoudiniAttributes::POINTID))
				SpecialAttributesColumnIndexes[EHoudiniAttributes::POINTID] = n;
		}
		else if ( CurrentTitle.Equals( TEXT("alive"), ESearchCase::IgnoreCase ) )
		{
			if (!IsValidAttributeColumnIndex(EHoudiniAttributes::ALIVE))
				SpecialAttributesColumnIndexes[EHoudiniAttributes::ALIVE] = n;
		}
		else if ( CurrentTitle.Equals( TEXT("life"), ESearchCase::IgnoreCase ) )
		{
			if (!IsValidAttributeColumnIndex(EHoudiniAttributes::LIFE))
				SpecialAttributesColumnIndexes[EHoudiniAttributes::LIFE] = n;
		}
		else if ( ( CurrentTitle.Equals( TEXT("Cd"), ESearchCase::IgnoreCase ) )
			|| ( CurrentTitle.Equals(TEXT("color"), ESearchCase::IgnoreCase ) ) )
		{
			if (!IsValidAttributeColumnIndex(EHoudiniAttributes::COLOR))
				SpecialAttributesColumnIndexes[EHoudiniAttributes::COLOR] = n;
		}
		else if ( ( CurrentTitle.Equals( TEXT("alpha"), ESearchCase::IgnoreCase ) )
			|| ( CurrentTitle.Equals(TEXT("A"), ESearchCase::IgnoreCase ) ) )
		{
			if (!IsValidAttributeColumnIndex(EHoudiniAttributes::ALPHA))
				SpecialAttributesColumnIndexes[EHoudiniAttributes::ALPHA] = n;
		}
		else if ( ( CurrentTitle.Equals( TEXT("v"), ESearchCase::IgnoreCase ) )
			|| ( CurrentTitle.Equals(TEXT("Vx"), ESearchCase::IgnoreCase ) ) )
		{
			if (!IsValidAttributeColumnIndex(EHoudiniAttributes::VELOCITY))
				SpecialAttributesColumnIndexes[EHoudiniAttributes::VELOCITY] = n;
		}
		else if ( CurrentTitle.Equals(TEXT("type"), ESearchCase::IgnoreCase ) )
		{
			if (!IsValidAttributeColumnIndex(EHoudiniAttributes::TYPE))
				SpecialAttributesColumnIndexes[EHoudiniAttributes::TYPE] = n;
		}
		else if ( CurrentTitle.Equals(TEXT("impulse"), ESearchCase::IgnoreCase ) )
		{
			if (!IsValidAttributeColumnIndex(EHoudiniAttributes::IMPULSE))
				SpecialAttributesColumnIndexes[EHoudiniAttributes::IMPULSE] = n;
		}
    }

	// Read the first row of the CSV file, and look for packed vectors value (X,Y,Z)
    // We'll have to expand them in the title row to match the parsed data
	HasPackedVectors = false;
	int32 FoundPackedVectorCharIndex = 0;    
    while ( FoundPackedVectorCharIndex != INDEX_NONE )
    {
		// Try to find ( in the row
		FoundPackedVectorCharIndex = FirstValueRow.Find( TEXT("("), ESearchCase::IgnoreCase, ESearchDir::FromStart, FoundPackedVectorCharIndex );
		if ( FoundPackedVectorCharIndex == INDEX_NONE )
			break;

		// We want to know which column this char belong to
		int32 FoundPackedVectorColumnIndex = INDEX_NONE;
		{
			// Chop the first row up to the found character
			FString FirstRowLeft = FirstValueRow.Left( FoundPackedVectorCharIndex );

			// ReplaceInLine returns the number of occurences of ",", that's what we want! 
			FoundPackedVectorColumnIndex = FirstRowLeft.ReplaceInline(TEXT(","), TEXT(""));
		}

		if ( !ColumnTitleArray.IsValidIndex( FoundPackedVectorColumnIndex ) )
		{
			UE_LOG( LogHoudiniNiagara, Warning,
			TEXT( "Error while parsing the CSV File. Couldn't unpack vector found at character %d in the first row!" ),
			FoundPackedVectorCharIndex + 1 );
			continue;
		}

		// We found a packed vector, get its size
		int32 FoundVectorSize = 0;
		{
			// Extract the vector string
			int32 FoundPackedVectorEndCharIndex = FirstValueRow.Find( TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromStart, FoundPackedVectorCharIndex );
			FString VectorString = FirstValueRow.Mid( FoundPackedVectorCharIndex + 1, FoundPackedVectorEndCharIndex - FoundPackedVectorCharIndex - 1 );

			// Use ReplaceInLine to count the number of , to get the vector's size!
			FoundVectorSize = VectorString.ReplaceInline( TEXT(","), TEXT("") ) + 1;
		}

		if ( FoundVectorSize < 2 )
			continue;

		// Increment the number of columns
		NumberOfColumns += ( FoundVectorSize - 1 );

		// Extract the special attributes column indexes we found
		int32 PositionColumnIndex = GetAttributeColumnIndex(EHoudiniAttributes::POSITION);
		int32 NormalColumnIndex = GetAttributeColumnIndex(EHoudiniAttributes::NORMAL);
		int32 ColorColumnIndex = GetAttributeColumnIndex(EHoudiniAttributes::COLOR);
		int32 AlphaColumnIndex = GetAttributeColumnIndex(EHoudiniAttributes::ALPHA);
		int32 VelocityColumnIndex = GetAttributeColumnIndex(EHoudiniAttributes::VELOCITY);		

		// Expand TitleRowArray
		if ( ( FoundPackedVectorColumnIndex == PositionColumnIndex ) && ( FoundVectorSize == 3 ) )
		{
			// Expand P to Px,Py,Pz
			ColumnTitleArray[ PositionColumnIndex ] = TEXT("Px");
			ColumnTitleArray.Insert( TEXT( "Py" ), PositionColumnIndex + 1 );
			ColumnTitleArray.Insert( TEXT( "Pz" ), PositionColumnIndex + 2 );
		}
		else if ( ( FoundPackedVectorColumnIndex == NormalColumnIndex ) && ( FoundVectorSize == 3 ) )
		{
			// Expand N to Nx,Ny,Nz
			ColumnTitleArray[ NormalColumnIndex ] = TEXT("Nx");
			ColumnTitleArray.Insert( TEXT("Ny"), NormalColumnIndex + 1 );
			ColumnTitleArray.Insert( TEXT("Nz"), NormalColumnIndex + 2 );
		}
		else if ( ( FoundPackedVectorColumnIndex == VelocityColumnIndex ) && ( FoundVectorSize == 3 ) )
		{
			// Expand V to Vx,Vy,Vz
			ColumnTitleArray[ VelocityColumnIndex ] = TEXT("Vx");
			ColumnTitleArray.Insert( TEXT("Vy"), VelocityColumnIndex + 1 );
			ColumnTitleArray.Insert( TEXT("Vz"), VelocityColumnIndex + 2 );
		}
		else if ( ( FoundPackedVectorColumnIndex == ColorColumnIndex ) && ( ( FoundVectorSize == 3 ) || ( FoundVectorSize == 4 ) ) )
		{
			// Expand Cd to R, G, B 
			ColumnTitleArray[ ColorColumnIndex ] = TEXT("R");
			ColumnTitleArray.Insert( TEXT("G"), ColorColumnIndex + 1 );
			ColumnTitleArray.Insert( TEXT("B"), ColorColumnIndex + 2 );

			if ( FoundVectorSize == 4 )
			{
				// Insert A if we had RGBA
				ColumnTitleArray.Insert( TEXT("A"), ColorColumnIndex + 3 );
				if ( AlphaColumnIndex == INDEX_NONE )
					AlphaColumnIndex = ColorColumnIndex + 3;
			}
		}
		else
		{
			// Expand the vector's title from V to V, V1, V2, V3 ...
			FString FoundPackedVectortTitle = ColumnTitleArray[ FoundPackedVectorColumnIndex ];
			for ( int32 n = 1; n < FoundVectorSize; n++ )
			{
				FString CurrentTitle = FoundPackedVectortTitle + FString::FromInt( n );
				ColumnTitleArray.Insert( CurrentTitle, FoundPackedVectorColumnIndex + n );
			}
		}

		// We have inserted new column titles because of the packed vector,
		// so we need to offset all the special attributes column indexes after the inserted values
		for ( int32 AttrIdx = EHoudiniAttributes::HOUDINI_ATTR_BEGIN; AttrIdx < EHoudiniAttributes::HOUDINI_ATTR_SIZE; AttrIdx++ )
		{
			if ( SpecialAttributesColumnIndexes[AttrIdx] == INDEX_NONE )
				continue;

			if ( SpecialAttributesColumnIndexes[AttrIdx] > FoundPackedVectorColumnIndex )
				SpecialAttributesColumnIndexes[AttrIdx] += FoundVectorSize - 1;
		}

		HasPackedVectors = true;
		FoundPackedVectorCharIndex++;
    }

    // For sanity, Check that the number of columns matches the title row and the first row
    {
		// Check the title row
		if ( NumberOfColumns != ColumnTitleArray.Num() )
			UE_LOG( LogHoudiniNiagara, Error,
			TEXT( "Error while parsing the CSV File. Found %d columns but the Title string has %d values! Some values will have an offset!" ),
			NumberOfColumns, ColumnTitleArray.Num() );

		// Use ReplaceInLine to count the number of columns in the first row and make sure it's correct
		FString FirstValueRowCopy = FirstValueRow;
		int32 FirstRowColumnCount = FirstValueRowCopy.ReplaceInline( TEXT(","), TEXT("") ) + 1;
		if ( NumberOfColumns != FirstRowColumnCount )
			UE_LOG( LogHoudiniNiagara, Error,
			TEXT("Error while parsing the CSV File. Found %d columns but found %d values in the first row! Some values will have an offset!" ),
			NumberOfColumns, FirstRowColumnCount );
    }
	/*
	// Update the TitleRow uproperty
	TitleRow.Empty();
	for (int32 n = 0; n < TitleRowArray.Num(); n++)
	{
		TitleRow += TitleRowArray[n];
		if ( n < TitleRowArray.Num() - 1 )
			TitleRow += TEXT(",");
	}
	*/	

	return true;
}


// Returns the float value at a given point in the CSV file
bool UHoudiniCSV::GetFloatValue( const int32& rowIndex, const int32& colIndex, float& value ) const
{
    if ( rowIndex < 0 || rowIndex >= NumberOfRows )
		return false;

    if ( colIndex < 0 || colIndex >= NumberOfColumns )
		return false;

    int32 Index = rowIndex + ( colIndex * NumberOfRows );
    if ( FloatCSVData.IsValidIndex( Index ) )
    {
		value = FloatCSVData[ Index ];
		return true;
    }

    return false;
}

/*
// Returns the float value at a given point in the CSV file
bool UHoudiniCSV::GetCSVStringValue( const int32& rowIndex, const int32& colIndex, FString& value )
{
    if ( rowIndex < 0 || rowIndex >= NumberOfRows )
		return false;

    if ( colIndex < 0 || colIndex >= NumberOfColumns )
		return false;

    int32 Index = rowIndex + ( colIndex * NumberOfRows );
    if ( StringCSVData.IsValidIndex( Index ) )
    {
		value = StringCSVData[ Index ];
		return true;
    }

    return false;
}
*/

// Returns a Vector 3 for a given point in the CSV file
bool UHoudiniCSV::GetVectorValue( const int32& rowIndex, const int32& colIndex, FVector& value, const bool& DoSwap, const bool& DoScale ) const
{
    FVector V = FVector::ZeroVector;
    if ( !GetFloatValue( rowIndex, colIndex, V.X ) )
		return false;

    if ( !GetFloatValue( rowIndex, colIndex + 1, V.Y ) )
		return false;

    if ( !GetFloatValue( rowIndex, colIndex + 2, V.Z ) )
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
bool UHoudiniCSV::GetPositionValue( const int32& rowIndex, FVector& value ) const
{
    FVector V = FVector::ZeroVector;
    if ( !GetVectorValue( rowIndex, GetAttributeColumnIndex(EHoudiniAttributes::POSITION), V, true, true ) )
		return false;

    value = V;

    return true;
}

// Returns a Vector 3 for a given point in the CSV file
bool UHoudiniCSV::GetNormalValue( const int32& rowIndex, FVector& value ) const
{
    FVector V = FVector::ZeroVector;
    if ( !GetVectorValue( rowIndex, GetAttributeColumnIndex(EHoudiniAttributes::NORMAL), V, true, false ) )
		return false;

    value = V;

    return true;
}

// Returns a time value for a given point in the CSV file
bool UHoudiniCSV::GetTimeValue( const int32& rowIndex, float& value ) const
{
    float temp;
    if ( !GetFloatValue( rowIndex, GetAttributeColumnIndex(EHoudiniAttributes::TIME), temp ) )
		return false;

    value = temp;

    return true;
}

// Returns a Color for a given point in the CSV file
bool UHoudiniCSV::GetColorValue( const int32& rowIndex, FLinearColor& value ) const
{
	FVector V = FVector::OneVector;
	if ( !GetVectorValue( rowIndex, GetAttributeColumnIndex(EHoudiniAttributes::COLOR), V, false, false ) )
		return false;

	FLinearColor C = FLinearColor::White;
	C.R = V.X;
	C.G = V.Y;
	C.B = V.Z;

	float alpha = 1.0f;
	if ( GetFloatValue( rowIndex, GetAttributeColumnIndex(EHoudiniAttributes::ALPHA), alpha ) )
		C.A = alpha;	

	value = C;

	return true;
}

// Returns a Velocity Vector3 for a given point in the CSV file
bool UHoudiniCSV::GetVelocityValue( const int32& rowIndex, FVector& value ) const
{
	FVector V = FVector::ZeroVector;
	if ( !GetVectorValue( rowIndex, GetAttributeColumnIndex(EHoudiniAttributes::VELOCITY), V, true, false ) )
		return false;

	value = V;

	return true;
}

// Returns an impulse value for a given point in the CSV file
bool UHoudiniCSV::GetImpulseValue(const int32& rowIndex, float& value) const
{
	float temp;
	if (!GetFloatValue(rowIndex, GetAttributeColumnIndex(EHoudiniAttributes::IMPULSE), temp))
		return false;

	value = temp;

	return true;
}

// Returns the number of points found in the CSV file
int32 UHoudiniCSV::GetNumberOfPoints() const
{
	if ( !IsValidAttributeColumnIndex(EHoudiniAttributes::POINTID) )
		return NumberOfPoints;

    return NumberOfRows;
}

// Returns the number of rows found in the CSV file
int32 UHoudiniCSV::GetNumberOfRows() const
{
	return NumberOfRows;
}

// Returns the number of columns found in the CSV file
int32 UHoudiniCSV::GetNumberOfColumns() const
{
	return NumberOfColumns;
}

// Get the last row index for a given time value (the row with a time smaller or equal to desiredTime)
// If the CSV file doesn't have time informations, returns false and set the LastRowIndex to the last row in the file
// If desiredTime is smaller than the time value in the first row, LastRowIndex will be set to -1
// If desiredTime is higher than the last time value in the last row of the csv file, LastIndex will be set to the last row's index
bool UHoudiniCSV::GetLastRowIndexAtTime(const float& desiredTime, int32& lastRowIndex) const
{
	// If we dont have proper time info, always return the last row
	int32 TimeColumnIndex = GetAttributeColumnIndex(EHoudiniAttributes::TIME);	
	if ( TimeColumnIndex < 0 || TimeColumnIndex >= NumberOfColumns )
	{
		lastRowIndex = NumberOfRows - 1;
		return false;
	}

	float temp_time = 0.0f;
	if ( GetTimeValue( NumberOfRows - 1, temp_time ) && temp_time < desiredTime )
	{
		// We didn't find a suitable index because the desired time is higher than our last time value
		lastRowIndex = NumberOfRows - 1;
		return true;
	}

	// Iterates through all the rows
	lastRowIndex = INDEX_NONE;
	for ( int32 n = 0; n < NumberOfRows; n++ )
	{
		if ( GetTimeValue( n, temp_time ) )
		{
			if ( temp_time == desiredTime )
			{
				lastRowIndex = n;
			}
			else if ( temp_time > desiredTime )
			{
				lastRowIndex = n - 1;
				return true;
			}
		}
	}

	// We didn't find a suitable index because the desired time is higher than our last time value
	if ( lastRowIndex == INDEX_NONE )
		lastRowIndex = NumberOfRows - 1;

	return true;
}
// Get the last index of the points with a time value smaller or equal to desiredTime
// If the CSV file doesn't have time informations, returns false and set the LastIndex to the last point
// If desiredTime is smaller than the first point time, LastIndex will be set to -1
// If desiredTime is higher than the last point time in the csv file, LastIndex will be set to the last point's index
bool UHoudiniCSV::GetLastPointIDToSpawnAtTime(const float& desiredTime, int32& lastID) const
{
	/*
	// If we dont have proper time info, always return the last point
	int32 TimeColumnIndex = GetAttributeColumnIndex(EHoudiniAttributes::TIME);
	if ( TimeColumnIndex < 0 || TimeColumnIndex >= NumberOfColumns )
	{
		lastID = NumberOfPoints - 1;
		return false;
	}
	*/

	float temp_time = 0.0f;
	if (!SpawnTimes.IsValidIndex(NumberOfPoints - 1))
	{
		lastID = NumberOfPoints - 1;
		LastDesiredTime = desiredTime;
		return false;
	}

	else if (SpawnTimes[NumberOfPoints - 1] < desiredTime && desiredTime > LastDesiredTime)
	{
		// We didn't find a suitable index because the desired time is higher than the last index's time value and the spawn time has not looped over
		lastID = NumberOfPoints - 1;
		LastDesiredTime = desiredTime;
		return true;
	}

	else
	{
		// Iterates through all the points
		lastID = GetNumberOfPoints();
		for (int32 n = 0; n < NumberOfPoints; n++)
		{
			temp_time = SpawnTimes[n];

			if (temp_time == desiredTime)
			{
				lastID = n;
			}
			else if (temp_time > desiredTime)
			{
				lastID = n - 1;
				LastDesiredTime = desiredTime;
				return true;
			}
		}
	}

	LastDesiredTime = desiredTime;
	return true;
}

bool UHoudiniCSV::GetPointType(const int32& PointID, int32& Value) const
{
	if ( !PointTypes.IsValidIndex( PointID ) )
	{
		Value = -1;
		return false;
	}

	Value = PointTypes[ PointID ];

	return true;
}

bool UHoudiniCSV::GetPointLife(const int32& PointID, float& Value) const
{
	if ( !LifeValues.IsValidIndex( PointID ) )
	{
		Value = -1.0f;
		return false;
	}

	Value = LifeValues[ PointID ];

	return true;
}

bool UHoudiniCSV::GetPointLifeAtTime( const int32& PointID, const float& DesiredTime, float& Value ) const
{
	if ( !SpawnTimes.IsValidIndex( PointID )  || !LifeValues.IsValidIndex( PointID ) )
	{
		Value = -1.0f;
		return false;
	}

	if ( DesiredTime < SpawnTimes[ PointID ] )
	{
		Value = LifeValues[ PointID ];
	}
	else
	{
		Value = LifeValues[ PointID ] - ( DesiredTime - SpawnTimes[ PointID ] );
	}

	return true;
}

bool UHoudiniCSV::GetPointValueAtTime( const int32& PointID, const int32& ColumnIndex, const float& desiredTime, float& Value ) const
{
	int32 PrevIndex = -1;
	int32 NextIndex = -1;
	float PrevWeight = 1.0f;

	if ( !GetRowIndexesForPointAtTime( PointID, desiredTime, PrevIndex, NextIndex, PrevWeight ) )
		return false;

	float PrevValue, NextValue;
	if ( !GetFloatValue( PrevIndex, ColumnIndex, PrevValue ) )
		return false;
	if ( !GetFloatValue( NextIndex, ColumnIndex, NextValue ) )
		return false;

	Value = FMath::Lerp( PrevValue, NextValue, PrevWeight );

	return true;
}

bool UHoudiniCSV::GetPointPositionAtTime( const int32& PointID, const float& desiredTime, FVector& Vector ) const
{
	FVector V = FVector::ZeroVector;
	if ( !GetPointVectorValueAtTime(PointID, GetAttributeColumnIndex(EHoudiniAttributes::POSITION), desiredTime, V, true, true ) )
		return false;

	Vector = V;

	return true;
}

bool UHoudiniCSV::GetPointVectorValueAtTime( const int32& PointID, const int32& ColumnIndex, const float& desiredTime, FVector& Vector, const bool& DoSwap, const bool& DoScale ) const
{
	int32 PrevIndex = -1;
	int32 NextIndex = -1;
	float PrevWeight = 1.0f;

	if ( !GetRowIndexesForPointAtTime( PointID, desiredTime, PrevIndex, NextIndex, PrevWeight ) )
		return false;

	FVector PrevVector, NextVector;
	if ( !GetVectorValue( PrevIndex, ColumnIndex, PrevVector, DoSwap, DoScale ) )
		return false;
	if ( !GetVectorValue( NextIndex, ColumnIndex, NextVector, DoSwap, DoScale ) )
		return false;

	Vector = FMath::Lerp(PrevVector, NextVector, PrevWeight);

	return true;
}

int32 UHoudiniCSV::GetMaxNumberOfPointValueIndexes() const
{
	int32 MaxNum = 0;
	for ( auto ValueIndexes : PointValueIndexes )
	{
		if ( MaxNum < ValueIndexes.RowIndexes.Num() )
			MaxNum = ValueIndexes.RowIndexes.Num();
	}

	return MaxNum;
}

bool UHoudiniCSV::GetRowIndexesForPointAtTime(const int32& PointID, const float& desiredTime, int32& PrevIndex, int32& NextIndex, float& PrevWeight ) const
{
	float PrevTime = -1.0f;
	float NextTime = -1.0f;

	// Invalid PointID
	if ( PointID < 0 || PointID >= NumberOfPoints )
		return false;

	// Get the row indexes for this point
	const TArray<int32>* RowIndexes = nullptr;
	if ( PointValueIndexes.IsValidIndex( PointID ) )
		RowIndexes = &( PointValueIndexes[ PointID ].RowIndexes );

	if ( !RowIndexes )
		return false;

	for ( auto n : *RowIndexes )
	{
		// Get the time
		float currentTime = -1.0f;
		if ( !GetTimeValue(n, currentTime) )
			currentTime = 0.0f;

		if ( currentTime == desiredTime )
		{
			PrevIndex = n;
			NextIndex = n;
			PrevWeight = 1.0f;
			return true;
		}
		else if ( currentTime < desiredTime )
		{
			if ( PrevTime == -1.0f || PrevTime < currentTime )
			{
				PrevIndex = n;
				PrevTime = currentTime;
			}
		}
		else
		{
			if ( NextTime == -1.0f || NextTime > currentTime)
			{
				NextIndex = n;
				NextTime = currentTime;

				// TODO: since the csv is sorted by time, we can break now
				break;
			}
		}
	}

	if ( PrevIndex < 0 && NextIndex < 0 )
		return false;

	if ( PrevIndex < 0 )
	{
		PrevWeight = 0.0f;
		PrevIndex = NextIndex;
		return true;
	}

	if ( NextIndex < 0 )
	{
		PrevWeight = 1.0f;
		NextIndex = PrevIndex;
		return true;
	}

	// Calculate the weight
	PrevWeight = ( ( desiredTime - PrevTime) / ( NextTime - PrevTime ) );

	return true;
}

bool UHoudiniCSV::GetPointIDsToSpawnAtTime(
	const float& desiredTime,
	int32& MinID, int32& MaxID, int32& Count,
	int32& LastSpawnedPointID, float& LastSpawnTime ) const
{
	int32 lastID = 0;
	if ( !GetLastPointIDToSpawnAtTime( desiredTime, lastID ) )
	{
		// The CSV file doesn't have time informations, so always return all points in the file
		MinID = 0;
		MaxID = lastID;
		Count = MaxID - MinID + 1;
	}
	else
	{
		// The CSV file has time informations
		// First, detect if we need to reset LastSpawnedPointID (after a loop of the emitter)
		if ( lastID < LastSpawnedPointID || desiredTime <= LastSpawnTime )
			LastSpawnedPointID = -1;

		if ( lastID < 0 )
		{
			// Nothing to spawn, t is lower than the point's time
			LastSpawnedPointID = -1;					
		}
		else
		{
			// The last time value in the CSV is lower than t, spawn everything if we didnt already!
			if ( lastID >= GetNumberOfPoints() )
				lastID = lastID - 1;

			if ( lastID == LastSpawnedPointID )
			{
				// We dont have any new point to spawn
				MinID = lastID;
				MaxID = lastID;
				Count = 0;
			}
			else
			{
				// We have points to spawn at time t
				MinID = LastSpawnedPointID + 1;
				MaxID = lastID;
				Count = MaxID - MinID + 1;

				LastSpawnedPointID = MaxID;
				LastSpawnTime = desiredTime;
			}
		}
	}

	return true;
}

int32 UHoudiniCSV::GetAttributeColumnIndex(const EHoudiniAttributes& Attr) const
{
	if ( !IsValidAttributeColumnIndex( Attr ) )
		return INDEX_NONE;

	return SpecialAttributesColumnIndexes[ Attr ];
}

// Returns if the specific attribute has a valid column index
bool UHoudiniCSV::IsValidAttributeColumnIndex(const EHoudiniAttributes& Attr) const
{
	if ( !SpecialAttributesColumnIndexes.IsValidIndex( Attr ) )
		return false;

	int32 ColIdx = SpecialAttributesColumnIndexes[ Attr ];
	if ( ( ColIdx < 0 ) || ( ColIdx >= NumberOfColumns) )
		return false;

	return true;
}

// Returns the column index for a given string
bool UHoudiniCSV::GetColumnIndexFromString( const FString& ColumnTitle, int32& ColumnIndex ) const
{
    if ( ColumnTitleArray.Find( ColumnTitle, ColumnIndex ) )
		return true;

    // Handling special attributes and packed positions/normals
    if ( ColumnTitle.Equals( "P", ESearchCase::IgnoreCase ) )
		return ColumnTitleArray.Find( TEXT( "Px" ), ColumnIndex );
    else if ( ColumnTitle.Equals( "N", ESearchCase::IgnoreCase ) )
		return ColumnTitleArray.Find( TEXT( "Nx" ), ColumnIndex );
	else if ( ColumnTitle.Equals("V", ESearchCase::IgnoreCase ) )
		return ColumnTitleArray.Find( TEXT("Vx"), ColumnIndex);
	else if ( ColumnTitle.Equals("Cd", ESearchCase::IgnoreCase ) )
		return ColumnTitleArray.Find( TEXT("R"), ColumnIndex);
	else if ( ColumnTitle.Equals("Alpha", ESearchCase::IgnoreCase ) )
		return ColumnTitleArray.Find( TEXT("A"), ColumnIndex);
	else
		return ColumnTitleArray.Find( ColumnTitle + TEXT("1"), ColumnIndex );

    return false;
}

// Returns the float value at a given point in the CSV file
bool UHoudiniCSV::GetFloatValueForString( const int32& rowIndex, const FString& ColumnTitle, float& value ) const
{
    int32 ColIndex = -1;
    if ( !GetColumnIndexFromString( ColumnTitle, ColIndex ) )
		return false;

    return GetFloatValue( rowIndex, ColIndex, value );
}

/*
// Returns the string value at a given point in the CSV file
bool UHoudiniCSV::GetCSVStringValue( const int32& rowIndex, const FString& ColumnTitle, FString& value )
{
    int32 ColIndex = -1;
    if ( !GetColumnIndexFromString( ColumnTitle, ColIndex ) )
		return false;

    return GetCSVStringValue( rowIndex, ColIndex, value );
}
*/

#if WITH_EDITORONLY_DATA
void
UHoudiniCSV::PostInitProperties()
{
    if ( !HasAnyFlags( RF_ClassDefaultObject ) )
    {
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
    }

    Super::PostInitProperties();
}

void
UHoudiniCSV::PostEditChangeProperty(FPropertyChangedEvent & PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if ( PropertyChangedEvent.GetPropertyName() == TEXT( "SourceTitleRow" ) )
	{
		UseCustomTitleRow = true;
		UpdateFromFile( FileName );
	}
	
}

void
UHoudiniCSV::GetAssetRegistryTags(TArray< FAssetRegistryTag > & OutTags) const
{
	// Add the source filename to the asset thumbnail tooltip
	OutTags.Add(FAssetRegistryTag("Source FileName", FileName, FAssetRegistryTag::TT_Alphabetical));

	// The Number of rows, columns and point found in the file
	OutTags.Add(FAssetRegistryTag("Number of Rows", FString::FromInt(NumberOfRows), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Number of Columns", FString::FromInt(NumberOfColumns), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Number of Points", FString::FromInt(NumberOfPoints), FAssetRegistryTag::TT_Numerical));

	// The source title row
	OutTags.Add(FAssetRegistryTag("Original Title Row", SourceTitleRow, FAssetRegistryTag::TT_Alphabetical));

	// The parsed column titles
	FString ParsedColTtiles;
	for ( int32 n = 0; n < ColumnTitleArray.Num(); n++ )
		ParsedColTtiles += TEXT("(") + FString::FromInt( n ) + TEXT(") ") + ColumnTitleArray[ n ] + TEXT(" ");

	OutTags.Add( FAssetRegistryTag( "Parsed Column Titles", ParsedColTtiles, FAssetRegistryTag::TT_Alphabetical ) );

	// And a list of the special attributes we found
	FString SpecialAttr;
	if (IsValidAttributeColumnIndex(EHoudiniAttributes::POINTID))
		SpecialAttr += TEXT("ID ");

	if (IsValidAttributeColumnIndex(EHoudiniAttributes::TYPE))
		SpecialAttr += TEXT("Type ");

	if (IsValidAttributeColumnIndex(EHoudiniAttributes::POSITION))
		SpecialAttr += TEXT("Position ");

	if (IsValidAttributeColumnIndex(EHoudiniAttributes::NORMAL))
		SpecialAttr += TEXT("Normal ");

	if (IsValidAttributeColumnIndex(EHoudiniAttributes::IMPULSE))
		SpecialAttr += TEXT("Impulse ");

	if (IsValidAttributeColumnIndex(EHoudiniAttributes::VELOCITY))
		SpecialAttr += TEXT("Velocity ");

	if (IsValidAttributeColumnIndex(EHoudiniAttributes::TIME))
		SpecialAttr += TEXT("Time ");

	if (IsValidAttributeColumnIndex(EHoudiniAttributes::COLOR))
		SpecialAttr += TEXT("Color ");

	if (IsValidAttributeColumnIndex(EHoudiniAttributes::ALPHA))
		SpecialAttr += TEXT("Alpha ");

	if (IsValidAttributeColumnIndex(EHoudiniAttributes::ALIVE))
		SpecialAttr += TEXT("Alive ");

	if (IsValidAttributeColumnIndex(EHoudiniAttributes::LIFE))
		SpecialAttr += TEXT("Life ");

	OutTags.Add(FAssetRegistryTag("Special Attributes", SpecialAttr, FAssetRegistryTag::TT_Alphabetical));

	Super::GetAssetRegistryTags( OutTags );
}
#endif

#undef LOCTEXT_NAMESPACE
