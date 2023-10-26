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

#include "HoudiniPointCache.h"

#include "HoudiniPointCacheLoaderBJSON.h"
#include "HoudiniPointCacheLoaderCSV.h"
#include "HoudiniPointCacheLoaderJSON.h"

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Math/NumericLimits.h"
#include "Misc/CoreMiscDefines.h" 
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PixelFormat.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"

#if WITH_EDITOR
	#include "EditorFramework/AssetImportData.h"
#endif

#define LOCTEXT_NAMESPACE "HoudiniNiagaraPointCacheAsset"

DEFINE_LOG_CATEGORY(LogHoudiniNiagara);
 

UHoudiniPointCache::UHoudiniPointCache( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer ),
	NumberOfSamples( -1 ),
	NumberOfAttributes( -1 ),
	NumberOfPoints( -1 ),
	NumberOfFrames( - 1 ),
	FirstFrame( FLT_MAX ),
	LastFrame( -FLT_MAX ),
	MinSampleTime( FLT_MAX ),
	MaxSampleTime( -FLT_MAX ),
	Resource(nullptr)
{
	SpecialAttributeIndexes.Init(INDEX_NONE, EHoudiniAttributes::HOUDINI_ATTR_SIZE);

	UseCustomCSVTitleRow = false;
	
#if WITH_EDITORONLY_DATA
	RawDataUncompressedSize = 0;
	RawDataCompressionMethod = NAME_None;
#endif
}

void UHoudiniPointCache::SetFileName( const FString& TheFileName )
{
    FileName = TheFileName;

	FString FileExt = FPaths::GetExtension(FileName).ToLower();
	if (FileExt == TEXT("HCSV"))
		FileType = EHoudiniPointCacheFileType::CSV;
	else if (FileExt == TEXT("HJSON"))
		FileType = EHoudiniPointCacheFileType::JSON;
	else if (FileExt == TEXT("HBJSON"))
		FileType = EHoudiniPointCacheFileType::BJSON;
	else
		FileType = EHoudiniPointCacheFileType::Invalid;
}

#if WITH_EDITOR
bool UHoudiniPointCache::UpdateFromFile( const FString& TheFileName )
{
    if ( TheFileName.IsEmpty() )
		return false;

    // Check that the file exists
    const FString FullFilePath = FPaths::ConvertRelativePathToFull( TheFileName );
    if ( !FPaths::FileExists( FullFilePath ) )
		return false;

	SetFileName(TheFileName);

	// Construct the loader based off of the file type
	TSharedPtr<FHoudiniPointCacheLoader> Loader;
	switch (FileType)
	{
		case EHoudiniPointCacheFileType::CSV:
			Loader = FHoudiniPointCacheLoaderCSV::Create<FHoudiniPointCacheLoaderCSV>(FileName);
			break;
		case EHoudiniPointCacheFileType::JSON:
			Loader = FHoudiniPointCacheLoaderJSON::Create<FHoudiniPointCacheLoaderJSON>(FileName);
			break;
		case EHoudiniPointCacheFileType::BJSON:
			Loader = FHoudiniPointCacheLoaderBJSON::Create<FHoudiniPointCacheLoaderBJSON>(FileName);
			break;
		default:
			return false;
	}

	if (!Loader)
		return false;

	return Loader->LoadToAsset(this);
}
#endif

// Returns the float value at a given point in the Point Cache
bool UHoudiniPointCache::GetFloatValue( const int32& sampleIndex, const int32& attrIndex, float& value ) const
{
    if ( sampleIndex < 0 || sampleIndex >= NumberOfSamples )
		return false;

    if ( attrIndex < 0 || attrIndex >= NumberOfAttributes )
		return false;

    int32 Index = sampleIndex + ( attrIndex * NumberOfSamples );
    if ( FloatSampleData.IsValidIndex( Index ) )
    {
		value = FloatSampleData[ Index ];
		return true;
    }

    return false;
}

/*
// Returns the float value at a given point in the CSV file
bool UHoudiniPointCache::GetCSVStringValue( const int32& sampleIndex, const int32& attrIndex, FString& value )
{
    if ( sampleIndex < 0 || sampleIndex >= NumberOfSamples )
		return false;

    if ( attrIndex < 0 || attrIndex >= NumberOfAttributes )
		return false;

    int32 Index = sampleIndex + ( attrIndex * NumberOfSamples );
    if ( StringCSVData.IsValidIndex( Index ) )
    {
		value = StringCSVData[ Index ];
		return true;
    }

    return false;
}
*/

// Returns a Vector 3 for a given point in the Point Cache
bool UHoudiniPointCache::GetVectorValue( const int32& sampleIndex, const int32& attrIndex, FVector& value, const bool& DoSwap, const bool& DoScale ) const
{
	FVector3f V = FVector3f::ZeroVector;
    if ( !GetFloatValue( sampleIndex, attrIndex, V.X ) )
		return false;

    if ( !GetFloatValue( sampleIndex, attrIndex + 1, V.Y ) )
		return false;

    if ( !GetFloatValue( sampleIndex, attrIndex + 2, V.Z ) )
		return false;

    if ( DoScale )
		V *= 100.0f;

    value = FVector(V.X, V.Y, V.Z);

    if ( DoSwap )
    {
		value.Y = V.Z;
		value.Z = V.Y;
    }

    return true;
}

// Returns the vector 3 value at a given point in the Point Cache
bool UHoudiniPointCache::GetVectorValueForString(const int32& sampleIndex, const FString& Attribute, FVector& value, const bool& DoSwap, const bool& DoScale) const
{
	int32 AttrIndex = -1;
	if (!GetAttributeIndexFromString(Attribute, AttrIndex))
		return false;

	return GetVectorValue(sampleIndex, AttrIndex, value, DoSwap, DoScale);
}

// Returns a Vector 4 for a given point in the Point Cache
bool UHoudiniPointCache::GetVector4Value( const int32& sampleIndex, const int32& attrIndex, FVector4& value ) const
{
	FVector4f V(0.f, 0.f, 0.f, 0.f);
    if ( !GetFloatValue( sampleIndex, attrIndex, V.X ) )
		return false;

    if ( !GetFloatValue( sampleIndex, attrIndex + 1, V.Y ) )
		return false;

    if ( !GetFloatValue( sampleIndex, attrIndex + 2, V.Z ) )
		return false;

    if ( !GetFloatValue( sampleIndex, attrIndex + 3, V.W ) )
		return false;

    value = (FVector4)V;

    return true;
}

// Returns the vector 4 value at a given point in the Point Cache
bool UHoudiniPointCache::GetVector4ValueForString(const int32& sampleIndex, const FString& Attribute, FVector4& value ) const
{
	int32 AttrIndex = -1;
	if (!GetAttributeIndexFromString(Attribute, AttrIndex))
		return false;

	return GetVector4Value(sampleIndex, AttrIndex, value);
}

// Returns a Quat for a given point in the Point Cache
bool UHoudiniPointCache::GetQuatValue( const int32& sampleIndex, const int32& attrIndex, FQuat& value, const bool& DoHoudiniToUnrealConversion ) const
{
    FQuat4f Q(0, 0, 0, 0);
    if ( !GetFloatValue( sampleIndex, attrIndex, Q.X ) )
		return false;

    if ( !GetFloatValue( sampleIndex, attrIndex + 1, Q.Y ) )
		return false;

    if ( !GetFloatValue( sampleIndex, attrIndex + 2, Q.Z ) )
		return false;

    if ( !GetFloatValue( sampleIndex, attrIndex + 3, Q.W ) )
		return false;

    value = FQuat(Q.X, Q.Y, Q.Z, Q.W);

    if ( DoHoudiniToUnrealConversion )
    {
		// Houdini Quat (y up, left handed) to unreal Quat (z up, right handed): swap y/z and negate
		value.X = -Q.X;
		value.Y = -Q.Z;
		value.Z = -Q.Y;
    }

    return true;
}

// Returns the quat value at a given point in the Point Cache
bool UHoudiniPointCache::GetQuatValueForString(const int32& sampleIndex, const FString& Attribute, FQuat& value, const bool& DoHoudiniToUnrealConversion ) const
{
	int32 AttrIndex = -1;
	if (!GetAttributeIndexFromString(Attribute, AttrIndex))
		return false;

	return GetQuatValue(sampleIndex, AttrIndex, value, DoHoudiniToUnrealConversion);
}

// Returns a Vector 3 for a given point in the Point Cache
bool UHoudiniPointCache::GetPositionValue( const int32& sampleIndex, FVector& value ) const
{
    FVector V = FVector::ZeroVector;
    if ( !GetVectorValue( sampleIndex, GetAttributeAttributeIndex(EHoudiniAttributes::POSITION), V, true, true ) )
		return false;

    value = V;

    return true;
}

// Returns a Vector 3 for a given point in the Point Cache
bool UHoudiniPointCache::GetNormalValue( const int32& sampleIndex, FVector& value ) const
{
    FVector V = FVector::ZeroVector;
    if ( !GetVectorValue( sampleIndex, GetAttributeAttributeIndex(EHoudiniAttributes::NORMAL), V, true, false ) )
		return false;

    value = V;

    return true;
}

// Returns a time value for a given point in the Point Cache
bool UHoudiniPointCache::GetTimeValue( const int32& sampleIndex, float& value ) const
{
    float temp;
    if ( !GetFloatValue( sampleIndex, GetAttributeAttributeIndex(EHoudiniAttributes::TIME), temp ) )
		return false;

    value = temp;

    return true;
}

// Returns a Color for a given point in the Point Cache
bool UHoudiniPointCache::GetColorValue( const int32& sampleIndex, FLinearColor& value ) const
{
	FVector V = FVector::OneVector;
	if ( !GetVectorValue( sampleIndex, GetAttributeAttributeIndex(EHoudiniAttributes::COLOR), V, false, false ) )
		return false;

	FLinearColor C = FLinearColor::White;
	C.R = V.X;
	C.G = V.Y;
	C.B = V.Z;

	float alpha = 1.0f;
	if ( GetFloatValue( sampleIndex, GetAttributeAttributeIndex(EHoudiniAttributes::ALPHA), alpha ) )
		C.A = alpha;	

	value = C;

	return true;
}

// Returns a Velocity Vector3 for a given point in the Point Cache
bool UHoudiniPointCache::GetVelocityValue( const int32& sampleIndex, FVector& value ) const
{
	FVector V = FVector::ZeroVector;
	if ( !GetVectorValue( sampleIndex, GetAttributeAttributeIndex(EHoudiniAttributes::VELOCITY), V, true, false ) )
		return false;

	value = V;

	return true;
}

// Returns an impulse value for a given point in the Point Cache
bool UHoudiniPointCache::GetImpulseValue(const int32& sampleIndex, float& value) const
{
	float temp;
	if (!GetFloatValue(sampleIndex, GetAttributeAttributeIndex(EHoudiniAttributes::IMPULSE), temp))
		return false;

	value = temp;

	return true;
}

// Returns the number of points found in the Point Cache
int32 UHoudiniPointCache::GetNumberOfPoints() const
{
	if ( !IsValidAttributeAttributeIndex(EHoudiniAttributes::POINTID) )
		return NumberOfPoints;

    return NumberOfSamples;
}

// Returns the number of samples found in the Point Cache
int32 UHoudiniPointCache::GetNumberOfSamples() const
{
	return NumberOfSamples;
}

// Returns the number of attributes found in the Point Cache
int32 UHoudiniPointCache::GetNumberOfAttributes() const
{
	return NumberOfAttributes;
}

// Get the last sample index for a given time value (the sample with a time smaller or equal to desiredTime)
// If the Point Cache doesn't have time informations, returns false and set the lastSampleIndex to the last sample in the file
// If desiredTime is smaller than the time value in the first sample, lastSampleIndex will be set to -1
// If desiredTime is higher than the last time value in the last sample of the Point Cache, lastSampleIndex will be set to the last sample's index
bool UHoudiniPointCache::GetLastSampleIndexAtTime(const float& desiredTime, int32& lastSampleIndex) const
{
	// If we dont have proper time info, always return the last sample index
	int32 TimeAttributeIndex = GetAttributeAttributeIndex(EHoudiniAttributes::TIME);	
	if ( TimeAttributeIndex < 0 || TimeAttributeIndex >= NumberOfAttributes )
	{
		lastSampleIndex = NumberOfSamples - 1;
		return false;
	}

	float temp_time = 0.0f;
	if ( GetTimeValue( NumberOfSamples - 1, temp_time ) && temp_time < desiredTime )
	{
		// We didn't find a suitable index because the desired time is higher than our last time value
		lastSampleIndex = NumberOfSamples - 1;
		return true;
	}

	// Iterates through all the samples
	lastSampleIndex = INDEX_NONE;
	for ( int32 n = 0; n < NumberOfSamples; n++ )
	{
		if ( GetTimeValue( n, temp_time ) )
		{
			if ( temp_time == desiredTime )
			{
				lastSampleIndex = n;
			}
			else if ( temp_time > desiredTime )
			{
				lastSampleIndex = n - 1;
				return true;
			}
		}
	}

	// We didn't find a suitable index because the desired time is higher than our last time value
	if ( lastSampleIndex == INDEX_NONE )
		lastSampleIndex = NumberOfSamples - 1;

	return true;
}

// Get the last index of the points with a time value smaller or equal to desiredTime
// If the Point Cache doesn't have time informations, returns false and set the LastIndex to the last point
// If desiredTime is smaller than the first point time, LastIndex will be set to -1
// If desiredTime is higher than the last point time in the Point Cache, LastIndex will be set to the last point's index
bool UHoudiniPointCache::GetLastPointIDToSpawnAtTime(const float& desiredTime, int32& lastID) const
{
	if (!SpawnTimes.IsValidIndex(NumberOfPoints - 1))
	{
		lastID = NumberOfPoints - 1;
		return false;
	}
	else if (SpawnTimes[NumberOfPoints - 1] < desiredTime)
	{
		// We didn't find a suitable index because the desired time is higher than the last index's time value and the spawn time has not looped over
		lastID = NumberOfPoints - 1;
		return true;
	}
	else
	{
		// Iterates through all the points to find the point who's spawn time exceeds the desired time.
		//lastID = GetNumberOfPoints();
		lastID = -1;
		for (int32 n = 0; n < NumberOfPoints; n++)
		{
			float SpawnTime = SpawnTimes[n];
			if (SpawnTime > desiredTime)
			{
				return true;
			}
			lastID = n;
		}
	}

	return true;
}

bool UHoudiniPointCache::GetPointType(const int32& PointID, int32& Value) const
{
	if ( !PointTypes.IsValidIndex( PointID ) )
	{
		Value = -1;
		return false;
	}

	Value = PointTypes[ PointID ];

	return true;
}

bool UHoudiniPointCache::GetPointLife(const int32& PointID, float& Value) const
{
	if ( !LifeValues.IsValidIndex( PointID ) )
	{
		Value = -1.0f;
		return false;
	}

	Value = LifeValues[ PointID ];

	return true;
}

bool UHoudiniPointCache::GetPointLifeAtTime( const int32& PointID, const float& DesiredTime, float& Value ) const
{
	if ( !SpawnTimes.IsValidIndex( PointID )  || !LifeValues.IsValidIndex( PointID ) )
	{
		Value = -1.0f;
		return false;
	}

	Value = LifeValues[ PointID ];

	return true;
}

bool UHoudiniPointCache::GetPointValueAtTime( const int32& PointID, const int32& AttributeIndex, const float& desiredTime, float& Value ) const
{
	int32 PrevSampleIndex = -1;
	int32 NextSampleIndex = -1;
	float PrevWeight = 1.0f;

	if ( !GetSampleIndexesForPointAtTime( PointID, desiredTime, PrevSampleIndex, NextSampleIndex, PrevWeight ) )
		return false;

	float PrevValue, NextValue;
	if ( !GetFloatValue( PrevSampleIndex, AttributeIndex, PrevValue ) )
		return false;
	if ( !GetFloatValue( NextSampleIndex, AttributeIndex, NextValue ) )
		return false;

	Value = FMath::Lerp( PrevValue, NextValue, PrevWeight );

	return true;
}

bool UHoudiniPointCache::GetPointValueAtTimeForString(const int32& PointID, const FString& Attribute, const float& desiredTime, float& Value) const
{
	int32 AttrIndex = -1;
	if (!GetAttributeIndexFromString(Attribute, AttrIndex))
		return false;

	return GetPointValueAtTime(PointID, AttrIndex, desiredTime, Value);
}

bool UHoudiniPointCache::GetPointPositionAtTime( const int32& PointID, const float& desiredTime, FVector& Vector ) const
{
	FVector V = FVector::ZeroVector;
	if ( !GetPointVectorValueAtTime(PointID, GetAttributeAttributeIndex(EHoudiniAttributes::POSITION), desiredTime, V, true, true ) )
		return false;

	Vector = V;

	return true;
}

bool UHoudiniPointCache::GetPointVectorValueAtTime( int32 PointID, int32 AttributeIndex, float desiredTime, FVector& Vector, bool DoSwap, bool DoScale ) const
{
	int32 PrevSampleIndex = -1;
	int32 NextSampleIndex = -1;
	float PrevWeight = 1.0f;

	if ( !GetSampleIndexesForPointAtTime( PointID, desiredTime, PrevSampleIndex, NextSampleIndex, PrevWeight ) )
		return false;

	FVector PrevVector, NextVector;
	if ( !GetVectorValue( PrevSampleIndex, AttributeIndex, PrevVector, DoSwap, DoScale ) )
		return false;
	if ( !GetVectorValue( NextSampleIndex, AttributeIndex, NextVector, DoSwap, DoScale ) )
		return false;

	Vector = FMath::Lerp(PrevVector, NextVector, PrevWeight);

	return true;
}

bool UHoudiniPointCache::GetPointVectorValueAtTimeForString(int32 PointID, const FString& Attribute, float desiredTime, FVector& Vector, bool DoSwap, bool DoScale) const
{
	int32 AttrIndex = -1;
	if (!GetAttributeIndexFromString(Attribute, AttrIndex))
		return false;

	return GetPointVectorValueAtTime(PointID, AttrIndex, desiredTime, Vector, DoSwap, DoScale);
}

bool UHoudiniPointCache::GetPointVector4ValueAtTime( int32 PointID, int32 AttributeIndex, float desiredTime, FVector4& Vector ) const
{
	int32 PrevSampleIndex = -1;
	int32 NextSampleIndex = -1;
	float PrevWeight = 1.0f;

	if ( !GetSampleIndexesForPointAtTime( PointID, desiredTime, PrevSampleIndex, NextSampleIndex, PrevWeight ) )
		return false;

	FVector4 PrevVector, NextVector;
	if ( !GetVector4Value( PrevSampleIndex, AttributeIndex, PrevVector ) )
		return false;
	if ( !GetVector4Value( NextSampleIndex, AttributeIndex, NextVector ) )
		return false;

	Vector = FMath::Lerp(PrevVector, NextVector, PrevWeight);

	return true;
}

bool UHoudiniPointCache::GetPointVector4ValueAtTimeForString(int32 PointID, const FString& Attribute, float desiredTime, FVector4& Vector ) const
{
	int32 AttrIndex = -1;
	if (!GetAttributeIndexFromString(Attribute, AttrIndex))
		return false;

	return GetPointVector4ValueAtTime(PointID, AttrIndex, desiredTime, Vector);
}

bool UHoudiniPointCache::GetPointQuatValueAtTime( int32 PointID, int32 AttributeIndex, float desiredTime, FQuat& Quat, bool DoHoudiniToUnrealConversion ) const
{
	int32 PrevSampleIndex = -1;
	int32 NextSampleIndex = -1;
	float PrevWeight = 1.0f;

	if ( !GetSampleIndexesForPointAtTime( PointID, desiredTime, PrevSampleIndex, NextSampleIndex, PrevWeight ) )
		return false;

	FQuat PrevQuat, NextQuat;
	if ( !GetQuatValue( PrevSampleIndex, AttributeIndex, PrevQuat, DoHoudiniToUnrealConversion ) )
		return false;
	if ( !GetQuatValue( NextSampleIndex, AttributeIndex, NextQuat, DoHoudiniToUnrealConversion ) )
		return false;

	Quat = FQuat::Slerp(PrevQuat, NextQuat, PrevWeight);

	return true;
}

bool UHoudiniPointCache::GetPointQuatValueAtTimeForString(int32 PointID, const FString& Attribute, float desiredTime, FQuat& Quat, bool DoHoudiniToUnrealConversion ) const
{
	int32 AttrIndex = -1;
	if (!GetAttributeIndexFromString(Attribute, AttrIndex))
		return false;

	return GetPointQuatValueAtTime(PointID, AttrIndex, desiredTime, Quat, DoHoudiniToUnrealConversion);
}

bool UHoudiniPointCache::GetPointFloatValueAtTime( int32 PointID, int32 AttributeIndex, float desiredTime, float& Value) const
{
	int32 PrevSampleIndex = -1;
	int32 NextSampleIndex = -1;
	float PrevWeight = 1.0f;

	if ( !GetSampleIndexesForPointAtTime( PointID, desiredTime, PrevSampleIndex, NextSampleIndex, PrevWeight ) )
		return false;

	float PrevValue, NextValue;
	if ( !GetFloatValue( PrevSampleIndex, AttributeIndex, PrevValue) )
		return false;
	if ( !GetFloatValue( NextSampleIndex, AttributeIndex, NextValue) )
		return false;

	Value = FMath::Lerp(PrevValue, NextValue, PrevWeight);

	return true;
}

bool UHoudiniPointCache::GetPointInt32ValueAtTime( int32 PointID, int32 AttributeIndex, float desiredTime, int32& Value) const
{
	int32 PrevSampleIndex = -1;
	int32 NextSampleIndex = -1;
	float PrevWeight = 1.0f;

	if ( !GetSampleIndexesForPointAtTime( PointID, desiredTime, PrevSampleIndex, NextSampleIndex, PrevWeight ) )
		return false;

	float FloatValue;
	if ( !GetFloatValue( PrevSampleIndex, AttributeIndex, FloatValue) )
		return false;

	Value = FMath::FloorToInt(FloatValue);
	return true;
}

int32 UHoudiniPointCache::GetMaxNumberOfPointValueIndexes() const
{
	int32 MaxNum = 0;
	for ( auto ValueIndexes : PointValueIndexes )
	{
		if ( MaxNum < ValueIndexes.SampleIndexes.Num() )
			MaxNum = ValueIndexes.SampleIndexes.Num();
	}

	return MaxNum;
}

bool UHoudiniPointCache::GetSampleIndexesForPointAtTime(const int32& PointID, const float& desiredTime, int32& PrevSampleIndex, int32& NextSampleIndex, float& PrevWeight ) const
{
	float PrevTime = -FLT_MAX;
	float NextTime = -FLT_MAX;

	// Invalid PointID
	if ( PointID < 0 || PointID >= NumberOfPoints )
		return false;

	// VA: Replace PointValueIndexes with TMAP for direct PointID lookups
	// Get the sample indexes for this point
	const TArray<int32>* SampleIndexes = nullptr;
	if ( PointValueIndexes.IsValidIndex( PointID ) )
		SampleIndexes = &( PointValueIndexes[ PointID ].SampleIndexes );

	if ( !SampleIndexes )
		return false;

	for ( auto n : *SampleIndexes )
	{
		// Get the time
		float currentTime = -FLT_MAX;
		if ( !GetTimeValue(n, currentTime) )
			currentTime = 0.0f;

		if ( FMath::IsNearlyEqual(currentTime, desiredTime) )
		{
			PrevSampleIndex = n;
			NextSampleIndex = n;
			PrevWeight = 1.0f;
			return true;
		}
		else if ( currentTime < desiredTime )
		{
			if ( FMath::IsNearlyEqual(PrevTime, -FLT_MAX) || PrevTime < currentTime )
			{
				PrevSampleIndex = n;
				PrevTime = currentTime;
			}
		}
		else
		{
			if ( FMath::IsNearlyEqual(NextTime, -FLT_MAX) || NextTime > currentTime)
			{
				NextSampleIndex = n;
				NextTime = currentTime;

				// TODO: since the csv is sorted by time, we can break now
				break;
			}
		}
	}

	if ( PrevSampleIndex < 0 && NextSampleIndex < 0 )
		return false;

	if ( PrevSampleIndex < 0 )
	{
		PrevWeight = 0.0f;
		PrevSampleIndex = NextSampleIndex;
		return true;
	}

	if ( NextSampleIndex < 0 )
	{
		PrevWeight = 1.0f;
		NextSampleIndex = PrevSampleIndex;
		return true;
	}

	// Calculate the weight
	PrevWeight = ( ( desiredTime - PrevTime) / ( NextTime - PrevTime ) );

	return true;
}

bool UHoudiniPointCache::GetPointIDsToSpawnAtTime(
	const float& desiredTime,
	int32& MinID, int32& MaxID, int32& Count,
	int32& LastSpawnedPointID, float& LastSpawnTime, float& LastSpawnTimeRequest) const
{
	int32 lastID = 0;
	if ( !GetLastPointIDToSpawnAtTime( desiredTime, lastID ) )
	{
		// The Point Cache doesn't have time informations, so always return all points in the file
		MinID = 0;
		MaxID = lastID;
		Count = MaxID - MinID + 1;
	}
	else
	{
		// The Point Cache has time informations
		// First, detect if we need to reset LastSpawnedPointID (after a loop of the emitter)
		 if ( lastID < LastSpawnedPointID || desiredTime <= LastSpawnTime || desiredTime <= LastSpawnTimeRequest )
		 	LastSpawnedPointID = -1;

		if ( lastID < 0 )
		{
			// Nothing to spawn, t is lower than the point's time
			LastSpawnedPointID = -1;
			MinID = lastID;
			MaxID = lastID;
			Count = 0;
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

	LastSpawnTimeRequest = desiredTime;

	return true;
}

bool UHoudiniPointCache::GetPointIDsToSpawnAtTime_DEPR(
	const float& desiredTime,
	int32& MinID, int32& MaxID, int32& Count,
	int32& LastSpawnedPointID, float& LastSpawnTime ) const
{
	int32 lastID = 0;
	if ( !GetLastPointIDToSpawnAtTime( desiredTime, lastID ) )
	{
		// The Point Cache doesn't have time informations, so always return all points in the file
		MinID = 0;
		MaxID = lastID;
		Count = MaxID - MinID + 1;
	}
	else
	{
		// The Point Cache has time informations
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

int32 UHoudiniPointCache::GetAttributeAttributeIndex(const EHoudiniAttributes& Attr) const
{
	if ( !IsValidAttributeAttributeIndex( Attr ) )
		return INDEX_NONE;

	return SpecialAttributeIndexes[ Attr ];
}

// Returns if the specific attribute has a valid attribute index
bool UHoudiniPointCache::IsValidAttributeAttributeIndex(const EHoudiniAttributes& Attr) const
{
	if ( !SpecialAttributeIndexes.IsValidIndex( Attr ) )
		return false;

	int32 ColIdx = SpecialAttributeIndexes[ Attr ];
	if ( ( ColIdx < 0 ) || ( ColIdx >= NumberOfAttributes) )
		return false;

	return true;
}

// Returns the attribute index for a given string and attribute name array
bool UHoudiniPointCache::GetAttributeIndexInArrayFromString(const FString& InAttribute, const TArray<FString>& InAttributeArray, int32& OutAttributeIndex)
{
    if ( InAttributeArray.Find( InAttribute, OutAttributeIndex ) )
		return true;

	const int32 DotIndex = InAttribute.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd, InAttribute.Len() - 1);
	if (DotIndex != INDEX_NONE)
		return false;

	// Search for the first attribute with a prefix match to InAttribute up to a .
	const uint32 NumAttributes = InAttributeArray.Num();
	const int32 PrefixLength = InAttribute.Len();
	for (uint32 Index = 0; Index < NumAttributes; ++Index)
	{
		const FString& Attribute = InAttributeArray[Index];
		if (Attribute.Len() > PrefixLength && Attribute.Mid(0, PrefixLength).Equals(InAttribute, ESearchCase::IgnoreCase) && Attribute[PrefixLength] == '.')
		{
			OutAttributeIndex = Index;
			return true;
		}
	}

    return false;
}

// Returns the attribute index for a given string
bool UHoudiniPointCache::GetAttributeIndexFromString( const FString& Attribute, int32& AttributeIndex ) const
{
   return GetAttributeIndexInArrayFromString(Attribute, AttributeArray, AttributeIndex);
}

// Returns the float value at a given point in the Point Cache
bool UHoudiniPointCache::GetFloatValueForString( const int32& sampleIndex, const FString& Attribute, float& value ) const
{
    int32 AttrIndex = -1;
    if ( !GetAttributeIndexFromString( Attribute, AttrIndex ) )
		return false;

    return GetFloatValue( sampleIndex, AttrIndex, value );
}

/*
// Returns the string value at a given point in the CSV file
bool UHoudiniPointCache::GetCSVStringValue( const int32& sampleIndex, const FString& Attribute, FString& value )
{
    int32 AttrIndex = -1;
    if ( !GetAttributeIndexFromString( Attribute, AttrIndex ) )
		return false;

    return GetCSVStringValue( sampleIndex, AttrIndex, value );
}
*/

#if WITH_EDITOR
void
UHoudiniPointCache::PostInitProperties()
{
    if ( !HasAnyFlags( RF_ClassDefaultObject ) )
    {
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
    }

    Super::PostInitProperties();
}
#endif

#if WITH_EDITOR
void
UHoudiniPointCache::PostEditChangeProperty(FPropertyChangedEvent & PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if ( PropertyChangedEvent.GetPropertyName() == TEXT( "SourceCSVTitleRow" ) )
	{
		UseCustomCSVTitleRow = true;
		UpdateFromFile( FileName );
	}
	
}
#endif

void
UHoudiniPointCache::GetAssetRegistryTags(TArray< FAssetRegistryTag > & OutTags) const
{
	// Add the source filename to the asset thumbnail tooltip
	OutTags.Add(FAssetRegistryTag("Source FileName", FileName, FAssetRegistryTag::TT_Alphabetical));

	// The Number of samples, attributes and points found in the file
	OutTags.Add(FAssetRegistryTag("Number of Samples", FString::FromInt(NumberOfSamples), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Number of Attributes", FString::FromInt(NumberOfAttributes), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Number of Points", FString::FromInt(NumberOfPoints), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Number of Frames (as exported)", FString::FromInt(NumberOfFrames), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("First Frame (as exported)", FString::FromInt(FirstFrame), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Last Frame (as exported)", FString::FromInt(LastFrame), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Minimum Sample Time (seconds)", FString::Printf(TEXT("%.4f"), MinSampleTime), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Maximum Sample Time (seconds)", FString::Printf(TEXT("%.4f"), MaxSampleTime), FAssetRegistryTag::TT_Numerical));
	const float MinSpawnTime = SpawnTimes.Num() > 0 ? SpawnTimes[0] : 0;
	OutTags.Add(FAssetRegistryTag("Minimum Spawn Time (seconds)", FString::Printf(TEXT("%.4f"), MinSpawnTime), FAssetRegistryTag::TT_Numerical));
	const float MaxSpawnTime = SpawnTimes.Num() > 0 ? SpawnTimes[SpawnTimes.Num() - 1] : 0;
	OutTags.Add(FAssetRegistryTag("Maximum Spawn Time (seconds)", FString::Printf(TEXT("%.4f"), MaxSpawnTime), FAssetRegistryTag::TT_Numerical));

	// The source title row
	OutTags.Add(FAssetRegistryTag("Original Title Row", SourceCSVTitleRow, FAssetRegistryTag::TT_Alphabetical));

	// The parsed attribute names
	FString ParsedAttributeNames;
	for ( int32 n = 0; n < AttributeArray.Num(); n++ )
		ParsedAttributeNames += TEXT("(") + FString::FromInt( n ) + TEXT(") ") + AttributeArray[ n ] + TEXT(" ");

	OutTags.Add( FAssetRegistryTag( "Parsed Attribute Names", ParsedAttributeNames, FAssetRegistryTag::TT_Alphabetical ) );

	// And a list of the special attributes we found
	FString SpecialAttr;
	if (IsValidAttributeAttributeIndex(EHoudiniAttributes::POINTID))
		SpecialAttr += TEXT("ID ");

	if (IsValidAttributeAttributeIndex(EHoudiniAttributes::TYPE))
		SpecialAttr += TEXT("Type ");

	if (IsValidAttributeAttributeIndex(EHoudiniAttributes::POSITION))
		SpecialAttr += TEXT("Position ");

	if (IsValidAttributeAttributeIndex(EHoudiniAttributes::NORMAL))
		SpecialAttr += TEXT("Normal ");

	if (IsValidAttributeAttributeIndex(EHoudiniAttributes::IMPULSE))
		SpecialAttr += TEXT("Impulse ");

	if (IsValidAttributeAttributeIndex(EHoudiniAttributes::VELOCITY))
		SpecialAttr += TEXT("Velocity ");

	if (IsValidAttributeAttributeIndex(EHoudiniAttributes::TIME))
		SpecialAttr += TEXT("Time ");

	if (IsValidAttributeAttributeIndex(EHoudiniAttributes::COLOR))
		SpecialAttr += TEXT("Color ");

	if (IsValidAttributeAttributeIndex(EHoudiniAttributes::ALPHA))
		SpecialAttr += TEXT("Alpha ");

	if (IsValidAttributeAttributeIndex(EHoudiniAttributes::LIFE))
		SpecialAttr += TEXT("Life ");

	if (IsValidAttributeAttributeIndex(EHoudiniAttributes::AGE))
		SpecialAttr += TEXT("Age ");

	OutTags.Add(FAssetRegistryTag("Special Attributes", SpecialAttr, FAssetRegistryTag::TT_Alphabetical));

	Super::GetAssetRegistryTags( OutTags );
}

void UHoudiniPointCache::BeginDestroy()
{
	Super::BeginDestroy();
	FHoudiniPointCacheResource* ThisResource = Resource.Get();
	ENQUEUE_RENDER_COMMAND(FHoudiniPointCache_ToRT) (
		[ThisResource](FRHICommandListImmediate& CmdList) mutable
	{
		if (ThisResource)
		{
			ThisResource->ReleaseResource();
		}
	}
	);
}


void UHoudiniPointCache::RequestPushToGPU()
{
	if (Resource != nullptr)
		return;

	Resource = MakeUnique<FHoudiniPointCacheResource>();

	TUniquePtr<FNiagaraDIHoudini_StaticDataPassToRT> DataToPass = MakeUnique<FNiagaraDIHoudini_StaticDataPassToRT>();
	//FMemory::Memzero(*DataToPass.Get());

	   
	DataToPass->NumSamples = GetNumberOfSamples();
	DataToPass->NumAttributes = GetNumberOfAttributes();
	DataToPass->NumPoints = GetNumberOfPoints();
	DataToPass->MaxNumIndexesPerPoint = GetMaxNumberOfPointValueIndexes() + 1;

	{
		uint32 NumElements = FloatSampleData.Num() ;
		if (NumElements > 0)
		{
			DataToPass->FloatData = (FloatSampleData);
			UE_LOG(LogScript, Warning, TEXT("HoudiniPointCacheAsset->FloatSampleData %d"), FloatSampleData.Num());
		}
	}

	{
		uint32 NumElements =  SpecialAttributeIndexes.Num() ;
		if (NumElements > 0)
		{
			DataToPass->SpecialAttributeIndexes = (SpecialAttributeIndexes);
		}
	}

	{
		uint32 NumElements = SpawnTimes.Num();
		if (NumElements > 0)
		{
			DataToPass->SpawnTimes = (SpawnTimes);
		}
	}

	{
		uint32 NumElements = LifeValues.Num();
		if (NumElements > 0)
		{
			DataToPass->LifeValues = (LifeValues);
		}
	}

	{
		uint32 NumElements =  PointTypes.Num();
		if (NumElements > 0)
		{
			DataToPass->PointTypes = (PointTypes);
		}
	}

	{
		uint32 NumPoints = PointValueIndexes.Num();
		// Add an extra to the max number so all indexes end by a -1
		uint32 MaxNumIndexesPerPoint = GetMaxNumberOfPointValueIndexes() + 1;
		uint32 NumElements = NumPoints * MaxNumIndexesPerPoint;

		if (NumElements > 0)
		{
			DataToPass->PointValueIndexes.Init(-1, NumElements);
			
			// We need to flatten the nested array for HLSL conversion
			for (int32 PointID = 0; PointID < PointValueIndexes.Num(); PointID++)
			{
				TArray<int32> SampleIndexes = PointValueIndexes[PointID].SampleIndexes;
				for (int32 Idx = 0; Idx < SampleIndexes.Num(); Idx++)
				{
					(DataToPass->PointValueIndexes)[PointID * MaxNumIndexesPerPoint + Idx] = SampleIndexes[Idx];
				}
			}
		}
	}

	{
		uint32 NumAttributes = GetNumberOfSamples();
		if (NumAttributes > 0)
		{
			DataToPass->Attributes = (AttributeArray);
		}
	}

	// Need to throw a ref count into the RHI buffer so that the resource is guaranteed to stay alive while in the queue.
	FHoudiniPointCacheResource* ThisResource = Resource.Get();

	ENQUEUE_RENDER_COMMAND(FHoudiniPointCache_ToRT) (
		[ThisResource, RTDataToPass = MoveTemp(DataToPass)](FRHICommandListImmediate& CmdList) mutable
		{
			if (ThisResource)
			{
				ThisResource->AcceptStaticDataUpdate(RTDataToPass);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
				ThisResource->InitResource(CmdList);
#else
				ThisResource->InitResource();
#endif
			}
		}
	);
}

void FHoudiniPointCacheResource::AcceptStaticDataUpdate(TUniquePtr<FNiagaraDIHoudini_StaticDataPassToRT>& Update)
{
	CachedData = MoveTemp(Update);
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
void FHoudiniPointCacheResource::InitRHI(FRHICommandListBase& RHICmdList)
#else
void FHoudiniPointCacheResource::InitRHI()
#endif
{
	if (!CachedData.IsValid())
		return;

	if (CachedData->FloatData.Num())
	{
		int32 NumElements = CachedData->FloatData.Num();
		FloatValuesGPUBuffer.Release();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		FloatValuesGPUBuffer.Initialize(RHICmdList, TEXT("HoudiniGPUBufferFloat"), sizeof(float), NumElements, EPixelFormat::PF_R32_FLOAT, BUF_Static);
#else
		FloatValuesGPUBuffer.Initialize(TEXT("HoudiniGPUBufferFloat"), sizeof(float), NumElements, EPixelFormat::PF_R32_FLOAT, BUF_Static);
#endif

		uint32 BufferSize = NumElements * sizeof(float);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		float* BufferData = static_cast<float*>(RHICmdList.LockBuffer(FloatValuesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
#else
		float* BufferData = static_cast<float*>(RHILockBuffer(FloatValuesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
#endif

		FPlatformMemory::Memcpy(BufferData, CachedData->FloatData.GetData(), BufferSize);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		RHICmdList.UnlockBuffer(FloatValuesGPUBuffer.Buffer);
#else
		RHIUnlockBuffer(FloatValuesGPUBuffer.Buffer);
#endif
	}

	if (CachedData->SpecialAttributeIndexes.Num())
	{
		uint32 NumElements = CachedData->SpecialAttributeIndexes.Num();

		SpecialAttributeIndexesGPUBuffer.Release();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		SpecialAttributeIndexesGPUBuffer.Initialize(RHICmdList, TEXT("HoudiniGPUBufferSpecialAttributeIndexes"), sizeof(int32), NumElements, EPixelFormat::PF_R32_SINT, BUF_Static);
#else
		SpecialAttributeIndexesGPUBuffer.Initialize(TEXT("HoudiniGPUBufferSpecialAttributeIndexes"), sizeof(int32), NumElements, EPixelFormat::PF_R32_SINT, BUF_Static);
#endif

		uint32 BufferSize = NumElements * sizeof(int32);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		float* BufferData = static_cast<float*>(RHICmdList.LockBuffer(SpecialAttributeIndexesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
#else
		float* BufferData = static_cast<float*>(RHILockBuffer(SpecialAttributeIndexesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
#endif

		FPlatformMemory::Memcpy(BufferData, CachedData->SpecialAttributeIndexes.GetData(), BufferSize);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		RHICmdList.UnlockBuffer(SpecialAttributeIndexesGPUBuffer.Buffer);
#else
		RHIUnlockBuffer(SpecialAttributeIndexesGPUBuffer.Buffer);
#endif
	}

	if (CachedData->SpawnTimes.Num())
	{
		uint32 NumElements = CachedData->SpawnTimes.Num();

		SpawnTimesGPUBuffer.Release();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		SpawnTimesGPUBuffer.Initialize(RHICmdList, TEXT("HoudiniGPUBufferSpawnTimes"), sizeof(float), NumElements, EPixelFormat::PF_R32_FLOAT, BUF_Static);
#else
		SpawnTimesGPUBuffer.Initialize(TEXT("HoudiniGPUBufferSpawnTimes"), sizeof(float), NumElements, EPixelFormat::PF_R32_FLOAT, BUF_Static);
#endif

		uint32 BufferSize = NumElements * sizeof(float);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		float* BufferData = static_cast<float*>(RHICmdList.LockBuffer(SpawnTimesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
#else
		float* BufferData = static_cast<float*>(RHILockBuffer(SpawnTimesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
#endif

		FPlatformMemory::Memcpy(BufferData, CachedData->SpawnTimes.GetData(), BufferSize);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		RHICmdList.UnlockBuffer(SpawnTimesGPUBuffer.Buffer);
#else
		RHIUnlockBuffer(SpawnTimesGPUBuffer.Buffer);
#endif
	}

	if (CachedData->LifeValues.Num())
	{
		uint32 NumElements = CachedData->LifeValues.Num();

		LifeValuesGPUBuffer.Release();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		LifeValuesGPUBuffer.Initialize(RHICmdList, TEXT("HoudiniGPUBufferLifeValues"), sizeof(float), NumElements, EPixelFormat::PF_R32_FLOAT, BUF_Static);
#else
		LifeValuesGPUBuffer.Initialize(TEXT("HoudiniGPUBufferLifeValues"), sizeof(float), NumElements, EPixelFormat::PF_R32_FLOAT, BUF_Static);
#endif

		uint32 BufferSize = NumElements * sizeof(float);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		float* BufferData = static_cast<float*>(RHICmdList.LockBuffer(LifeValuesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
#else
		float* BufferData = static_cast<float*>(RHILockBuffer(LifeValuesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
#endif

		FPlatformMemory::Memcpy(BufferData, CachedData->LifeValues.GetData(), BufferSize);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		RHICmdList.UnlockBuffer(LifeValuesGPUBuffer.Buffer);
#else
		RHIUnlockBuffer(LifeValuesGPUBuffer.Buffer);
#endif
	}

	if (CachedData->PointTypes.Num())
	{
		uint32 NumElements = CachedData->PointTypes.Num();

		PointTypesGPUBuffer.Release();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		PointTypesGPUBuffer.Initialize(RHICmdList, TEXT("HoudiniGPUBufferPointTypes"), sizeof(int32), NumElements, EPixelFormat::PF_R32_SINT, BUF_Static);
#else
		PointTypesGPUBuffer.Initialize(TEXT("HoudiniGPUBufferPointTypes"), sizeof(int32), NumElements, EPixelFormat::PF_R32_SINT, BUF_Static);
#endif

		uint32 BufferSize = NumElements * sizeof(int32);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		int32* BufferData = static_cast<int32*>(RHICmdList.LockBuffer(PointTypesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
#else
		int32* BufferData = static_cast<int32*>(RHILockBuffer(PointTypesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
#endif

		FPlatformMemory::Memcpy(BufferData, CachedData->PointTypes.GetData(), BufferSize);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		RHICmdList.UnlockBuffer(PointTypesGPUBuffer.Buffer);
#else
		RHIUnlockBuffer(PointTypesGPUBuffer.Buffer);
#endif
	}

	if (CachedData->PointValueIndexes.Num())
	{
		uint32 NumElements = CachedData->PointValueIndexes.Num();

		PointValueIndexesGPUBuffer.Release();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		PointValueIndexesGPUBuffer.Initialize(RHICmdList, TEXT("HoudiniGPUBufferPointValuesIndexes"), sizeof(int32), NumElements, EPixelFormat::PF_R32_SINT, BUF_Static);
#else
		PointValueIndexesGPUBuffer.Initialize(TEXT("HoudiniGPUBufferPointValuesIndexes"), sizeof(int32), NumElements, EPixelFormat::PF_R32_SINT, BUF_Static);
#endif

		uint32 BufferSize = NumElements * sizeof(int32);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		int32* BufferData = static_cast<int32*>(RHICmdList.LockBuffer(PointValueIndexesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
#else
		int32* BufferData = static_cast<int32*>(RHILockBuffer(PointValueIndexesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
#endif
		FPlatformMemory::Memcpy(BufferData, CachedData->PointValueIndexes.GetData(), BufferSize);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		RHICmdList.UnlockBuffer(PointValueIndexesGPUBuffer.Buffer);
#else
		RHIUnlockBuffer(PointValueIndexesGPUBuffer.Buffer);
#endif
	}

	if (CachedData->Attributes.Num())
	{
		Attributes = (CachedData->Attributes);
	}
	else
	{
		Attributes.Empty();
	}

	NumSamples = CachedData->NumSamples;
	NumAttributes = CachedData->NumAttributes;
	NumPoints = CachedData->NumPoints;
	MaxNumberOfIndexesPerPoint = CachedData->MaxNumIndexesPerPoint;

	CachedData.Reset();
}

void FHoudiniPointCacheResource::ReleaseRHI()
{
	FloatValuesGPUBuffer.Release();
	SpecialAttributeIndexesGPUBuffer.Release();
	SpawnTimesGPUBuffer.Release();
	LifeValuesGPUBuffer.Release();
	PointTypesGPUBuffer.Release();
	PointValueIndexesGPUBuffer.Release();
}	
#undef LOCTEXT_NAMESPACE