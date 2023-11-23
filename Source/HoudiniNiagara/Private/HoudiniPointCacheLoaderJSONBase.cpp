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

#include "HoudiniPointCacheLoaderJSONBase.h"

#include "HoudiniPointCache.h"

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CoreMiscDefines.h" 
#include "Misc/Paths.h"
#include "ShaderCompiler.h"


FHoudiniPointCacheLoaderJSONBase::FHoudiniPointCacheLoaderJSONBase(const FString& InFilePath) :
    FHoudiniPointCacheLoader(InFilePath)
{

}


FHoudiniPointCacheLoaderJSONBase::~FHoudiniPointCacheLoaderJSONBase()
{

}


bool FHoudiniPointCacheLoaderJSONBase::ParseAttributesAndInitAsset(UHoudiniPointCache *InAsset, const FHoudiniPointCacheJSONHeader &InHeader)
{
    // Get the relevant point cache asset data arrays
    TArray<FString> &AttributeArray = InAsset->AttributeArray;
    TArray<int32> &SpecialAttributeIndexes = InAsset->GetSpecialAttributeIndexes();

	// Reset the indexes of the special attributes
	SpecialAttributeIndexes.Init(INDEX_NONE, EHoudiniAttributes::HOUDINI_ATTR_SIZE);

    InAsset->NumberOfAttributes = InHeader.NumAttributeComponents;
    InAsset->AttributeArray.Empty(InAsset->NumberOfAttributes);

    // Look for the position, normal and time attributes indexes
    uint32 AttrIndex = 0;
    for (uint32 HeaderAttrIndex = 0; HeaderAttrIndex < InHeader.NumAttributes; ++HeaderAttrIndex)
    {
        uint32 AttrSize = InHeader.AttributeSizes[HeaderAttrIndex];
		// Remove spaces from the attribute name
        FString AttrName = InHeader.Attributes[HeaderAttrIndex];
		AttrName.ReplaceInline( TEXT(" "), TEXT("") );

        bool bIsVector = false;
        bool bIsColor = false;
		if (AttrName.Equals(TEXT("P"), ESearchCase::IgnoreCase))
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::POSITION))
				SpecialAttributeIndexes[EHoudiniAttributes::POSITION] = AttrIndex;
            bIsVector = true;
		}
		else if ( AttrName.Equals(TEXT("N"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::NORMAL))
				SpecialAttributeIndexes[EHoudiniAttributes::NORMAL] = AttrIndex;
            bIsVector = true;
		}
		else if ( AttrName.Contains( TEXT("time"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::TIME))
				SpecialAttributeIndexes[EHoudiniAttributes::TIME] = AttrIndex;
		}
		else if ( AttrName.Equals( TEXT("id"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::POINTID))
				SpecialAttributeIndexes[EHoudiniAttributes::POINTID] = AttrIndex;
		}
		else if ( AttrName.Equals( TEXT("life"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::LIFE))
				SpecialAttributeIndexes[EHoudiniAttributes::LIFE] = AttrIndex;
		}
		else if ( AttrName.Equals( TEXT("age"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::AGE))
				SpecialAttributeIndexes[EHoudiniAttributes::AGE] = AttrIndex;
		}
		else if ( AttrName.Equals(TEXT("Cd"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::COLOR))
				SpecialAttributeIndexes[EHoudiniAttributes::COLOR] = AttrIndex;
            bIsColor = true;
		}
		else if ( AttrName.Equals( TEXT("alpha"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::ALPHA))
				SpecialAttributeIndexes[EHoudiniAttributes::ALPHA] = AttrIndex;
		}
		else if ( AttrName.Equals(TEXT("v"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::VELOCITY))
				SpecialAttributeIndexes[EHoudiniAttributes::VELOCITY] = AttrIndex;
            bIsVector = true;
		}
		else if ( AttrName.Equals(TEXT("type"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::TYPE))
				SpecialAttributeIndexes[EHoudiniAttributes::TYPE] = AttrIndex;
		}
		else if ( AttrName.Equals(TEXT("impulse"), ESearchCase::IgnoreCase ) )
		{
			if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::IMPULSE))
				SpecialAttributeIndexes[EHoudiniAttributes::IMPULSE] = AttrIndex;
		}

        if (AttrSize > 1)
        {
            const TCHAR VectorSuffixes[4] = {'x', 'y', 'z', 'w'};
            const TCHAR ColorSuffixes[4] = {'r', 'g', 'b', 'a'};
            for (uint32 ComponentIndex = 0; ComponentIndex < AttrSize; ++ComponentIndex)
            {
                if (bIsVector) 
                {
                    AttributeArray.Add(FString::Printf(TEXT("%s.%c"), *AttrName, VectorSuffixes[ComponentIndex]));
                }
                else if (bIsColor)
                {
                    AttributeArray.Add(FString::Printf(TEXT("%s.%c"), *AttrName, ColorSuffixes[ComponentIndex]));
                }
                else
                {
                    AttributeArray.Add(FString::Printf(TEXT("%s.%d"), *AttrName, ComponentIndex + 1));
                }
            }
        }
        else
        {
            AttributeArray.Add(AttrName);
        }        

        AttrIndex += AttrSize;
    }

    // If there was no time attribute in the file, add it
    if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::TIME))
    {
        InAsset->AttributeArray.Add(TEXT("time"));
        InAsset->NumberOfAttributes = InAsset->AttributeArray.Num();
        SpecialAttributeIndexes[EHoudiniAttributes::TIME] = InAsset->NumberOfAttributes - 1;
    }

    // Get references to the various data arrays of the asset and initialize them
    TArray<float> &FloatSampleData = InAsset->GetFloatSampleData();
    TArray<float> &SpawnTimes = InAsset->GetSpawnTimes();
    TArray<float> &LifeValues = InAsset->GetLifeValues();
    TArray<int32> &PointTypes = InAsset->GetPointTypes();
    TArray<FPointIndexes> &PointValueIndexes = InAsset->GetPointValueIndexes();

    // Pre-allocate arrays in the point cache asset based off of the header
    InAsset->NumberOfPoints = InHeader.NumPoints;
    InAsset->NumberOfSamples = InHeader.NumSamples;
    InAsset->NumberOfFrames = InHeader.NumFrames;
    InAsset->FirstFrame = FLT_MAX;
    InAsset->LastFrame = -FLT_MAX;
    InAsset->MinSampleTime = FLT_MAX;
    InAsset->MaxSampleTime = -FLT_MAX;

    FloatSampleData.Empty(InAsset->NumberOfSamples * InAsset->NumberOfAttributes);
    FloatSampleData.Init(-FLT_MAX, InAsset->NumberOfSamples * InAsset->NumberOfAttributes);
    SpawnTimes.Empty(InAsset->NumberOfPoints);
    SpawnTimes.Init(-FLT_MAX, InAsset->NumberOfPoints);
    LifeValues.Empty(InAsset->NumberOfPoints);
    LifeValues.Init(-FLT_MAX, InAsset->NumberOfPoints);
    PointTypes.Empty(InAsset->NumberOfPoints);
    PointTypes.Init(-1, InAsset->NumberOfPoints);
    PointValueIndexes.Empty(InAsset->NumberOfPoints);
    PointValueIndexes.Init(FPointIndexes(), InAsset->NumberOfPoints);

	return true;
}


bool FHoudiniPointCacheLoaderJSONBase::ProcessFrame(UHoudiniPointCache *InAsset, float InFrameNumber, const TArray<TArray<float>> &InFrameData, float InFrameTime, uint32 InFrameStartSampleIndex, uint32 InNumPointsInFrame, uint32 InNumAttributesPerPoint, const FHoudiniPointCacheJSONHeader &InHeader, TMap<int32, int32>& InHoudiniIDToNiagaraIDMap, int32 &OutNextPointID) const
{
    // Get references to the various data arrays of the asset
    TArray<float> &FloatSampleData = InAsset->GetFloatSampleData();
    TArray<float> &SpawnTimes = InAsset->GetSpawnTimes();
    TArray<float> &LifeValues = InAsset->GetLifeValues();
    TArray<int32> &PointTypes = InAsset->GetPointTypes();
    TArray<FPointIndexes> &PointValueIndexes = InAsset->GetPointValueIndexes();

    // Set Min/Max Time seen in asset
    if (InFrameTime < InAsset->MinSampleTime)
    {
        InAsset->MinSampleTime = InFrameTime;
    }
    if (InFrameTime > InAsset->MaxSampleTime)
    {
        InAsset->MaxSampleTime = InFrameTime;
    }
    if (InFrameNumber < InAsset->FirstFrame)
    {
        InAsset->FirstFrame = InFrameNumber;
    }
    if (InFrameNumber > InAsset->LastFrame)
    {
        InAsset->LastFrame = InFrameNumber;
    }

    // Get Age attribute index, we'll use this to ensure we sort point spawn time correctly
    int32 IDAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::POINTID);
	int32 AgeAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::AGE);
    int32 LifeAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::LIFE);
    int32 TypeAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::TYPE);
    int32 TimeAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::TIME);

    if (InFrameData.Num() != InNumPointsInFrame)
    {
        UE_LOG(LogHoudiniNiagara, Error, TEXT("Inconsistent InFrameData size vs specified number of points in frame."));
        return false;
    }

    // Copy the frame data into the FloatSampleData array, determine unique points IDs
    // Also calculate SpawnTimes, LifeValues (if the life attribute exists)
    int32 CurrentID = -1;
    for (uint32 FrameSampleIndex = 0; FrameSampleIndex < InNumPointsInFrame; ++FrameSampleIndex)
    {
        uint32 SampleIndex = InFrameStartSampleIndex + FrameSampleIndex;
        // Check Attribute sample array size
        if (InFrameData[FrameSampleIndex].Num() != InNumAttributesPerPoint)
        {
            UE_LOG(LogHoudiniNiagara, Error, TEXT("Inconsistent InFrameData at SampleIndex %d: point attribute array size vs specified number of attributes per point."), SampleIndex);
            return false;
        }
        for (uint32 AttrIndex = 0; AttrIndex < InNumAttributesPerPoint; ++AttrIndex)
        {
            // Get the float value for the attribute
            float FloatValue = InFrameData[FrameSampleIndex][AttrIndex];

            // Handle point IDs here
            if (AttrIndex == IDAttributeIndex)
            {
                // If the point ID doesn't exist in the Houdini/Niagara mapping, create a new a entry.
                // Otherwise, replace the point ID with the Niagara ID.
                int32 PointID = FMath::FloorToInt(FloatValue);

                // The point ID may need to be replaced
                if (!InHoudiniIDToNiagaraIDMap.Contains(PointID))
                {
                    // We found a new point, so we add it to the ID map
                    InHoudiniIDToNiagaraIDMap.Add(PointID, OutNextPointID++);

                    // // Add a new array for that point's indexes
                    // PointValueIndexes.Add(FPointIndexes());
                }

                // Get the Niagara ID from the Houdini ID
                CurrentID = InHoudiniIDToNiagaraIDMap[PointID];

                // Check that CurrentID is still in the expected range
                if (CurrentID < 0 || CurrentID >= InAsset->NumberOfPoints)
                {
                    // ID is out of range
                    UE_LOG(LogHoudiniNiagara, Error, TEXT("Generated out of range point ID %d, expected max number of points %d"), CurrentID, InAsset->NumberOfPoints);
                    return false;
                }

                FloatValue = static_cast<float>(CurrentID);

                // Add the current sample index to this point's sample index list
                PointValueIndexes[CurrentID].SampleIndexes.Add(SampleIndex);
            }

            // Store the Value in the buffer
            FloatSampleData[SampleIndex + (AttrIndex * InAsset->NumberOfSamples)] = FloatValue;
        }

        // Always use the frame time, in other words, ignore a 'time' attribute
        if (TimeAttributeIndex != INDEX_NONE)
        {
            FloatSampleData[SampleIndex + (TimeAttributeIndex * InAsset->NumberOfSamples)] = InFrameTime;
        }

        // If we dont have Point ID information, we still want to fill the PointValueIndexes array
        if (IDAttributeIndex == INDEX_NONE)
        {
            // Each sample is considered its own point
            // PointValueIndexes.Add(FPointIndexes());
            PointValueIndexes[SampleIndex].SampleIndexes.Add(SampleIndex);
        }

        // Calculate SpawnTimes, LifeValues and PointTypes
        // Get the reconstructed point id
        if (IDAttributeIndex != INDEX_NONE)
        {
            // Get remapped the external point ID to the expected Niagara particle ID.
            CurrentID = static_cast<int32>(FloatSampleData[SampleIndex + (IDAttributeIndex * InAsset->NumberOfSamples)]);
            // CurrentID = InHoudiniIDToNiagaraIDMap[CurrentID];
        }
        else
        {
            CurrentID = SampleIndex;
        }
        
        // The time value comes from the frame entry
        const float CurrentTime = InFrameTime;

        if (FMath::IsNearlyEqual(SpawnTimes[CurrentID], -FLT_MAX))
        {
            // We have detected a new particle. 

            // Calculate spawn and life from attributes.
            // Spawn time is when the point is first seen
            if (AgeAttributeIndex != INDEX_NONE)
            {
                // If we have an age attribute we can more accurately calculate the particle's spawn time.
                SpawnTimes[CurrentID] = CurrentTime - FloatSampleData[SampleIndex + (AgeAttributeIndex * InAsset->NumberOfSamples)];
            }
            else 
            {
                // We don't have an age attribute. Simply use the current time as the spawn time.
                // Note that we bias the value slightly to that the particle is already spawned at the CurrentTime.
                SpawnTimes[CurrentID] = CurrentTime;
            }

            if (LifeAttributeIndex != INDEX_NONE)
            {	
                LifeValues[CurrentID] = FloatSampleData[SampleIndex + (LifeAttributeIndex * InAsset->NumberOfSamples)];
            }
        }

        // If we don't have a life attribute, keep setting the life attribute for the particle
        // so that when the particle is no longer present we have recorded its last observed time
        if (LifeAttributeIndex == INDEX_NONE && LifeValues[CurrentID] < CurrentTime)
        {
            // Life is the difference between spawn time and time of death
            // Note that the particle should still be alive at this timestep. Since we don't 
            // have access to a frame rate here we will workaround this for now by adding
            // a small value such that the particle dies *after* this timestep.
            LifeValues[CurrentID] = CurrentTime - SpawnTimes[CurrentID];
        }		

        // Keep track of the point type at spawn
        if (PointTypes[CurrentID] < 0)
        {
            float CurrentType = 0.0f;
            if (TypeAttributeIndex != INDEX_NONE)
                CurrentType = FloatSampleData[SampleIndex + (TypeAttributeIndex * InAsset->NumberOfSamples)];

            PointTypes[CurrentID] = static_cast<int32>(CurrentType);
        }
    }

    return true;
}