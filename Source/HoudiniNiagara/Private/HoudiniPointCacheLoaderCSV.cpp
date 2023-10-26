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

#include "HoudiniPointCacheLoaderCSV.h"

#include "HoudiniPointCache.h"

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CoreMiscDefines.h" 
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"


FHoudiniPointCacheLoaderCSV::FHoudiniPointCacheLoaderCSV(const FString& InFilePath)
	: FHoudiniPointCacheLoader(InFilePath)
{

}

#if WITH_EDITOR
bool FHoudiniPointCacheLoaderCSV::LoadToAsset(UHoudiniPointCache *InAsset)
{
    // Parse the file to a string array
    TArray<FString> StringArray;
    if (!FFileHelper::LoadFileToStringArray(StringArray, *GetFilePath()))
		return false;
	
    if (!UpdateFromStringArray(InAsset, StringArray))
    {
	    return false;
    }

    // Load uncompressed raw data into asset.
	if (!LoadRawPointCacheData(InAsset, *GetFilePath()))
	{
		return false;
	}
	// Finalize load by compressing raw data.
	CompressRawData(InAsset);

	return true;
}
#endif

#if WITH_EDITOR
bool FHoudiniPointCacheLoaderCSV::UpdateFromStringArray(UHoudiniPointCache *InAsset, TArray<FString>& InStringArray)
{
    if (!InAsset)
    {
        UE_LOG(LogHoudiniNiagara, Error, TEXT("Cannot load CSV file to HoudiniPointCache, InAsset is NULL."));
        return false;
    }

    // Reset the CSV sizes
    InAsset->NumberOfAttributes = 0;
    InAsset->NumberOfSamples = 0;
	InAsset->NumberOfPoints = 0;

    // Get references to point cache data arrays
    TArray<float> &FloatSampleData = InAsset->GetFloatSampleData();
    TArray<float> &SpawnTimes = InAsset->GetSpawnTimes();
    TArray<float> &LifeValues = InAsset->GetLifeValues();
    TArray<int32> &PointTypes = InAsset->GetPointTypes();
    TArray<int32> &SpecialAttributeIndexes = InAsset->GetSpecialAttributeIndexes();
    TArray<FPointIndexes> &PointValueIndexes = InAsset->GetPointValueIndexes();

	// Reset the column indexes of the special attributes
	SpecialAttributeIndexes.Init( INDEX_NONE, EHoudiniAttributes::HOUDINI_ATTR_SIZE );

    if ( InStringArray.Num() <= 0 )
    {
		UE_LOG( LogHoudiniNiagara, Error, TEXT( "Could not load the CSV file, error: not enough rows in the file." ) );
		return false;
    }

    // Remove empty rows from the CSV
    InStringArray.RemoveAll( [&]( const FString& InString ) { return InString.IsEmpty(); } );

    // Number of rows in the CSV (ignoring the title row)
    InAsset->NumberOfSamples = InStringArray.Num() - 1;
    if ( InAsset->NumberOfSamples < 1 )
    {
		UE_LOG( LogHoudiniNiagara, Error, TEXT( "Could not load the CSV file, error: not enough rows in the file." ) );
		return false;
    }

	// See if we need to use a custom title row
	// The custom title row will be ignored if it is empty or only composed of spaces
	FString TitleRow = InAsset->SourceCSVTitleRow;
	TitleRow.ReplaceInline(TEXT(" "), TEXT(""));
	if ( TitleRow.IsEmpty() )
		InAsset->SetUseCustomCSVTitleRow(false);

	if ( !InAsset->GetUseCustomCSVTitleRow() )
		InAsset->SourceCSVTitleRow = InStringArray[0];

	// Parses the CSV file's title row to update the column indexes of special values we're interested in
	// Also look for packed vectors in the first row and update the indexes accordingly
	bool HasPackedVectors = false;
	if ( !ParseCSVTitleRow( InAsset, InAsset->SourceCSVTitleRow, InStringArray[1], HasPackedVectors ) )
		return false;
    
    // Remove the title row now that it's been processed
    InStringArray.RemoveAt( 0 );

	// Parses each string of the csv file to a string array
	TArray< TArray< FString > > ParsedStringArrays;
	ParsedStringArrays.SetNum( InAsset->NumberOfSamples );
	for ( int32 rowIdx = 0; rowIdx < InAsset->NumberOfSamples; rowIdx++ )
	{
		// Get the current row
		FString CurrentRow = InStringArray[ rowIdx ];
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
		if ( InAsset->NumberOfAttributes != CurrentParsedRow.Num() )
			UE_LOG( LogHoudiniNiagara, Warning,
			TEXT("Error while parsing the CSV File. Row %d has %d values instead of the expected %d!"),
			rowIdx + 1, CurrentParsedRow.Num(), InAsset->NumberOfAttributes );

		// Store the parsed row
		ParsedStringArrays[ rowIdx ] = CurrentParsedRow;
	}

	// If we have time and/or age values, we have to make sure the csv rows are sorted by time and/or age
	int32 TimeAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::TIME);
	int32 AgeAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::AGE);
	int32 IDAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::POINTID);
	if ( TimeAttributeIndex != INDEX_NONE )
	{
		// First check if we need to sort the array
		bool NeedToSort = false;
		float PreviousTimeValue = 0.0f;
		float PreviousAgeValue = 0.0f;
		for ( int32 rowIdx = 0; rowIdx < ParsedStringArrays.Num(); rowIdx++ )
		{
			if ( !ParsedStringArrays[ rowIdx ].IsValidIndex( TimeAttributeIndex ) && 
				 !ParsedStringArrays[ rowIdx ].IsValidIndex( AgeAttributeIndex ) )
				continue;

			// Get the current time value
			float CurrentTimeValue = PreviousTimeValue;
			if (ParsedStringArrays[rowIdx].IsValidIndex(TimeAttributeIndex))
			{
				CurrentTimeValue = FCString::Atof(*(ParsedStringArrays[rowIdx][TimeAttributeIndex]));
			}

			// Get the current age value
			float CurrentAgeValue = PreviousAgeValue;
			if (ParsedStringArrays[rowIdx].IsValidIndex(AgeAttributeIndex))
			{
				CurrentAgeValue = FCString::Atof(*(ParsedStringArrays[rowIdx][AgeAttributeIndex]));
			}

			if ( rowIdx == 0 )
			{
				PreviousTimeValue = CurrentTimeValue;
				PreviousAgeValue = CurrentAgeValue;
				continue;
			}

			// Time values arent sorted properly
			if ( PreviousTimeValue > CurrentTimeValue || (PreviousTimeValue == CurrentTimeValue && PreviousAgeValue < CurrentAgeValue ) )
			{
				NeedToSort = true;
				break;
			}
		}

		if ( NeedToSort )
		{
			// We need to sort the CSV rows by their time values
			ParsedStringArrays.Sort<FHoudiniPointCacheSortPredicate>( FHoudiniPointCacheSortPredicate( TimeAttributeIndex, AgeAttributeIndex, IDAttributeIndex ) );
		}
	}

    // Initialize our different buffers
    FloatSampleData.Empty();
    FloatSampleData.SetNumZeroed( InAsset->NumberOfSamples * InAsset->NumberOfAttributes );

	/*
    StringCSVData.Empty();
    StringCSVData.SetNumZeroed( NumberOfSamples * NumberOfAttributes );
	*/
	// Due to the way that some of the DI functions work,
	// we expect that the point IDs start at zero, and increment as the points are spawned
	// Make sure this is the case by converting the point IDs as we read them
	int32 NextPointID = 0;
	TMap<int32, int32> HoudiniIDToNiagaraIDMap;
	

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
		for ( int colIdx = 0; colIdx < InAsset->NumberOfAttributes; colIdx++ )
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
			if ( colIdx == IDAttributeIndex )
			{
				// If the point ID doesn't exist in the Houdini/Niagara mapping, create a new a entry.
				// Otherwise, replace the point ID with the Niagara ID.

				int32 PointID = FMath::FloorToInt(FloatValue);
				// The point ID may need to be replaced
				if ( !HoudiniIDToNiagaraIDMap.Contains( PointID ) )
				{
					// We found a new point, so we add it to the ID map
					HoudiniIDToNiagaraIDMap.Add( PointID, NextPointID++ );

					// Add a new array for that point's indexes
					PointValueIndexes.Add( FPointIndexes() );
				}

				// Get the Niagara ID from the Houdini ID
				CurrentID = HoudiniIDToNiagaraIDMap[ PointID ];
				FloatValue = (float)CurrentID;

				// Add the current row to this point's row index list
				PointValueIndexes[ CurrentID ].SampleIndexes.Add( rowIdx );
			}

			// Store the Value in the buffer
			FloatSampleData[ rowIdx + ( colIdx * InAsset->NumberOfSamples ) ] = FloatValue;
		}

		// If we dont have Point ID informations, we still want to fill the PointValueIndexes array
		if ( !InAsset->IsValidAttributeAttributeIndex( EHoudiniAttributes::POINTID ) )
		{
			// Each row is considered its own point
			PointValueIndexes.Add(FPointIndexes());
			PointValueIndexes[ rowIdx ].SampleIndexes.Add( rowIdx );
		}
    }
	
	InAsset->NumberOfPoints = HoudiniIDToNiagaraIDMap.Num();
	if ( InAsset->NumberOfPoints <= 0 )
		InAsset->NumberOfPoints = InAsset->NumberOfSamples;

	// Look for point specific attributes to build some helper arrays
	int32 LifeAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::LIFE);
	//int32 AgeAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::AGE);
	int32 TypeAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::TYPE);

	SpawnTimes.Empty();
	SpawnTimes.Init( -FLT_MAX, InAsset->NumberOfPoints );

	LifeValues.Empty();
	LifeValues.Init( -FLT_MAX,  InAsset->NumberOfPoints );

	PointTypes.Empty();
	PointTypes.Init( -1, InAsset->NumberOfPoints );
	
	const float SpawnBias = 0.001f;
	for ( int rowIdx = 0; rowIdx < InAsset->NumberOfSamples; rowIdx++ )
	{
		// Get the point ID
		int32 CurrentID = rowIdx;
		if ( IDAttributeIndex != INDEX_NONE )
		{
			// Remap the external point ID to the expected Niagara particle ID.
			CurrentID = (int32)FloatSampleData[ rowIdx + ( IDAttributeIndex * InAsset->NumberOfSamples ) ];
			CurrentID = HoudiniIDToNiagaraIDMap[CurrentID];
		}

		// Get the time value for the current row
		float CurrentTime = 0.0f;
		if ( TimeAttributeIndex != INDEX_NONE )
			CurrentTime = FloatSampleData[ rowIdx + ( TimeAttributeIndex * InAsset->NumberOfSamples ) ];

		if ( FMath::IsNearlyEqual(SpawnTimes[ CurrentID ], -FLT_MAX) )
		{
			// We have detected a new particle. 

			// Calculate spawn and life from attributes.
			// Spawn time is when the point is first seen
			if (AgeAttributeIndex != INDEX_NONE)
			{
				// If we have an age attribute we can more accurately calculate the particle's spawn time.
				SpawnTimes[ CurrentID ] = CurrentTime - FloatSampleData [rowIdx + (AgeAttributeIndex * InAsset->NumberOfSamples) ];
			}
			else 
			{
				// We don't have an age attribute. Simply use the current time as the spawn time.
				// Note that we bias the value slightly to that the particle is already spawned at the CurrentTime.
				SpawnTimes[ CurrentID ] = CurrentTime;
			}

			if ( LifeAttributeIndex != INDEX_NONE )
			{	
				LifeValues[ CurrentID ] = FloatSampleData [rowIdx + (LifeAttributeIndex * InAsset->NumberOfSamples) ];
			}
		}

		// If we don't have a life attribute, keep setting the life attribute for the particle
		// so that when the particle is no longer present we have recorded its last observed time
		if ( LifeAttributeIndex == INDEX_NONE && LifeValues[ CurrentID ] < CurrentTime)
		{
			// Life is the difference between spawn time and time of death
			// Note that the particle should still be alive at this timestep. Since we don't 
			// have access to a frame rate here we will workaround this for now by adding
			// a small value such that the particle dies *after* this timestep.
			LifeValues[ CurrentID ] = CurrentTime - SpawnTimes[ CurrentID ];
		}		

		// Keep track of the point type at spawn
		if ( PointTypes[ CurrentID ] < 0 )
		{
			float CurrentType = 0.0f;
			if (TypeAttributeIndex != INDEX_NONE)
				InAsset->GetFloatValue( rowIdx, TypeAttributeIndex, CurrentType );

			PointTypes[ CurrentID ] = (int32)CurrentType;
		}

	}
    return true;
}
#endif

#if WITH_EDITOR
bool FHoudiniPointCacheLoaderCSV::ParseCSVTitleRow( UHoudiniPointCache *InAsset, const FString& TitleRow, const FString& FirstValueRow, bool& HasPackedVectors )
{
    // Get the relevant point cache asset data arrays
    TArray<FString> &AttributeArray = InAsset->AttributeArray;
    TArray<int32> &SpecialAttributeIndexes = InAsset->GetSpecialAttributeIndexes();
    
	// Get the number of values per row via the title row
    AttributeArray.Empty();
    TitleRow.ParseIntoArray( AttributeArray, TEXT(",") );
    InAsset->NumberOfAttributes = AttributeArray.Num();
    if ( InAsset->NumberOfAttributes < 1 )
    {
		UE_LOG( LogHoudiniNiagara, Error, TEXT( "Could not load the CSV file, error: not enough columns." ) );
		return false;
    }

    // Look for the position, normal and time attributes indexes
    for ( int32 n = 0; n < AttributeArray.Num(); n++ )
    {
		// Remove spaces from the title row
		AttributeArray[ n ].ReplaceInline( TEXT(" "), TEXT("") );

		FString CurrentTitle = AttributeArray[ n ];
		if ( CurrentTitle.Equals( TEXT("P.x"), ESearchCase::IgnoreCase ) || CurrentTitle.Equals(TEXT("P"), ESearchCase::IgnoreCase ) )
		{
			if ( !InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::POSITION))
				SpecialAttributeIndexes[EHoudiniAttributes::POSITION] = n;
		}
		else if ( CurrentTitle.Equals( TEXT("N.x"), ESearchCase::IgnoreCase ) || CurrentTitle.Equals(TEXT("N"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::NORMAL))
				SpecialAttributeIndexes[EHoudiniAttributes::NORMAL] = n;
		}
		else if ( CurrentTitle.Contains( TEXT("time"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::TIME))
				SpecialAttributeIndexes[EHoudiniAttributes::TIME] = n;
		}
		else if ( CurrentTitle.Equals( TEXT("id"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::POINTID))
				SpecialAttributeIndexes[EHoudiniAttributes::POINTID] = n;
		}
		else if ( CurrentTitle.Equals( TEXT("life"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::LIFE))
				SpecialAttributeIndexes[EHoudiniAttributes::LIFE] = n;
		}
		else if ( CurrentTitle.Equals( TEXT("age"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::AGE))
				SpecialAttributeIndexes[EHoudiniAttributes::AGE] = n;
		}
		else if ( CurrentTitle.Equals( TEXT("Cd.r"), ESearchCase::IgnoreCase ) || CurrentTitle.Equals(TEXT("Cd"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::COLOR))
				SpecialAttributeIndexes[EHoudiniAttributes::COLOR] = n;
		}
		else if ( CurrentTitle.Equals( TEXT("alpha"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::ALPHA))
				SpecialAttributeIndexes[EHoudiniAttributes::ALPHA] = n;
		}
		else if ( CurrentTitle.Equals( TEXT("v.x"), ESearchCase::IgnoreCase ) || CurrentTitle.Equals(TEXT("v"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::VELOCITY))
				SpecialAttributeIndexes[EHoudiniAttributes::VELOCITY] = n;
		}
		else if ( CurrentTitle.Equals(TEXT("type"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::TYPE))
				SpecialAttributeIndexes[EHoudiniAttributes::TYPE] = n;
		}
		else if ( CurrentTitle.Equals(TEXT("impulse"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::IMPULSE))
				SpecialAttributeIndexes[EHoudiniAttributes::IMPULSE] = n;
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
		int32 FoundPackedVectorAttributeIndex = INDEX_NONE;
		{
			// Chop the first row up to the found character
			FString FirstRowLeft = FirstValueRow.Left( FoundPackedVectorCharIndex );

			// ReplaceInLine returns the number of occurences of ",", that's what we want! 
			FoundPackedVectorAttributeIndex = FirstRowLeft.ReplaceInline(TEXT(","), TEXT(""));
		}

		if ( !AttributeArray.IsValidIndex( FoundPackedVectorAttributeIndex ) )
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
		InAsset->NumberOfAttributes += ( FoundVectorSize - 1 );

		// Extract the special attributes column indexes we found
		int32 PositionAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::POSITION);
		int32 NormalAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::NORMAL);
		int32 ColorAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::COLOR);
		int32 AlphaAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::ALPHA);
		int32 VelocityAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::VELOCITY);		

		// Expand TitleRowArray
		if ( ( FoundPackedVectorAttributeIndex == PositionAttributeIndex ) && ( FoundVectorSize == 3 ) )
		{
			// Expand P to Px,Py,Pz
			AttributeArray[ PositionAttributeIndex ] = TEXT("P.x");
			AttributeArray.Insert( TEXT( "P.y" ), PositionAttributeIndex + 1 );
			AttributeArray.Insert( TEXT( "P.z" ), PositionAttributeIndex + 2 );
		}
		else if ( ( FoundPackedVectorAttributeIndex == NormalAttributeIndex ) && ( FoundVectorSize == 3 ) )
		{
			// Expand N to Nx,Ny,Nz
			AttributeArray[ NormalAttributeIndex ] = TEXT("N.x");
			AttributeArray.Insert( TEXT("N.y"), NormalAttributeIndex + 1 );
			AttributeArray.Insert( TEXT("N.z"), NormalAttributeIndex + 2 );
		}
		else if ( ( FoundPackedVectorAttributeIndex == VelocityAttributeIndex ) && ( FoundVectorSize == 3 ) )
		{
			// Expand V to Vx,Vy,Vz
			AttributeArray[ VelocityAttributeIndex ] = TEXT("v.x");
			AttributeArray.Insert( TEXT("v.y"), VelocityAttributeIndex + 1 );
			AttributeArray.Insert( TEXT("v.z"), VelocityAttributeIndex + 2 );
		}
		else if ( ( FoundPackedVectorAttributeIndex == ColorAttributeIndex ) && ( ( FoundVectorSize == 3 ) || ( FoundVectorSize == 4 ) ) )
		{
			// Expand Cd to R, G, B 
			AttributeArray[ ColorAttributeIndex ] = TEXT("Cd.r");
			AttributeArray.Insert( TEXT("Cd.g"), ColorAttributeIndex + 1 );
			AttributeArray.Insert( TEXT("Cd.b"), ColorAttributeIndex + 2 );

			if ( FoundVectorSize == 4 )
			{
				// Insert A if we had RGBA
				AttributeArray.Insert( TEXT("Cd.a"), ColorAttributeIndex + 3 );
				if ( AlphaAttributeIndex == INDEX_NONE )
					AlphaAttributeIndex = ColorAttributeIndex + 3;
			}
		}
		else
		{
			// Expand the vector's title from V to V, V1, V2, V3 ...
			FString FoundPackedVectortTitle = AttributeArray[ FoundPackedVectorAttributeIndex ];
			for ( int32 n = 0; n < FoundVectorSize; n++ )
			{
				FString CurrentTitle = FString::Printf( TEXT( "%s.%d" ), *FoundPackedVectortTitle, n );
				if (n == 0)
					AttributeArray[FoundPackedVectorAttributeIndex] = CurrentTitle;
				else
					AttributeArray.Insert( CurrentTitle, FoundPackedVectorAttributeIndex + n );
			}
		}

		// We have inserted new column titles because of the packed vector,
		// so we need to offset all the special attributes column indexes after the inserted values
		for ( int32 AttrIdx = EHoudiniAttributes::HOUDINI_ATTR_BEGIN; AttrIdx < EHoudiniAttributes::HOUDINI_ATTR_SIZE; AttrIdx++ )
		{
			if ( SpecialAttributeIndexes[AttrIdx] == INDEX_NONE )
				continue;

			if ( SpecialAttributeIndexes[AttrIdx] > FoundPackedVectorAttributeIndex )
				SpecialAttributeIndexes[AttrIdx] += FoundVectorSize - 1;
		}

		HasPackedVectors = true;
		FoundPackedVectorCharIndex++;
    }

    // For sanity, Check that the number of columns matches the title row and the first row
    {
		// Check the title row
		if ( InAsset->NumberOfAttributes != AttributeArray.Num() )
			UE_LOG( LogHoudiniNiagara, Error,
			TEXT( "Error while parsing the CSV File. Found %d columns but the Title string has %d values! Some values will have an offset!" ),
			InAsset->NumberOfAttributes, AttributeArray.Num() );

		// Use ReplaceInLine to count the number of columns in the first row and make sure it's correct
		FString FirstValueRowCopy = FirstValueRow;
		int32 FirstRowColumnCount = FirstValueRowCopy.ReplaceInline( TEXT(","), TEXT("") ) + 1;
		if ( InAsset->NumberOfAttributes != FirstRowColumnCount )
			UE_LOG( LogHoudiniNiagara, Error,
			TEXT("Error while parsing the CSV File. Found %d columns but found %d values in the first row! Some values will have an offset!" ),
			InAsset->NumberOfAttributes, FirstRowColumnCount );
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
#endif