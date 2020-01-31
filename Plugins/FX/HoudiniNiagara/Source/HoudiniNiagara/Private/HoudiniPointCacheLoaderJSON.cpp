#include "HoudiniPointCacheLoaderJSON.h"
#include "HoudiniPointCache.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"

const TCHAR FHoudiniPointCacheLoaderJSON::MarkerTypeNull = 'Z';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerTypeNoop = 'N';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerTypeBoolTrue = 'T';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerTypeBoolFalse = 'F';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerTypeInt8 = 'i';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerTypeUInt8 = 'U';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerTypeInt16 = 'I';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerTypeInt32 = 'l';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerTypeInt64 = 'L';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerTypeFloat32 = 'd';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerTypeFloat64 = 'D';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerTypeHighPrec = 'H';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerTypeChar = 'C';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerTypeString = 'S';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerObjectStart = '{';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerObjectEnd = '}';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerArrayStart = '[';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerArrayEnd = ']';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerContainerType = '$';
const TCHAR FHoudiniPointCacheLoaderJSON::MarkerContainerCount = '#';

struct FHoudiniPointCacheJSONHeader 
{
    FString Version;
    uint32 NumSamples;
    uint32 NumFrames;
    uint32 NumPoints;
    uint32 NumAttributes;
    TArray<FString> Attributes;
    TArray<uint8> AttributeSizes;
    FString DataType;
};

FHoudiniPointCacheLoaderJSON::FHoudiniPointCacheLoaderJSON(const FString& InFilePath) :
    FHoudiniPointCacheLoader(InFilePath)
{

}

bool FHoudiniPointCacheLoaderJSON::LoadToAsset(UHoudiniPointCache *InAsset)
{
    const FString& InFilePath = GetFilePath();
	FScopedLoadingState ScopedLoadingState(*InFilePath);

	TSharedPtr<FArchive> ReaderPtr(IFileManager::Get().CreateFileReader(*InFilePath));
	if (!ReaderPtr)
	{
	    UE_LOG(LogHoudiniNiagara, Warning, TEXT("Failed to read file '%s' error."), *InFilePath);
		return false;
	}

    FArchive &Reader = *ReaderPtr;
    // Data in the file is in big endian
    Reader.SetByteSwapping(true);

    // Read start of root object
    TArray<uint8> Buffer;
    Buffer.SetNumZeroed(1024);
    TCHAR Marker = '\0';
    if (!ReadMarker(Reader, Buffer, Marker) || Marker != MarkerObjectStart)
    {
        UE_LOG(LogHoudiniNiagara, Error, TEXT("Expected root object start."));
        return false;
    }
    
    FString ObjectKey;
    if (!ReadNonContainerValue(Reader, Buffer, ObjectKey, false))
        return false;
    if (ObjectKey != TEXT("header"))
        return false;

    // Found header
    FHoudiniPointCacheJSONHeader Header;
    if (!ReadHeader(Reader, Buffer, Header))
    {
        UE_LOG(LogHoudiniNiagara, Error, TEXT("Could not read header."));
        return false;
    }

    // Pre-allocate arrays in the point cache asset based off of the header
    InAsset->NumberOfPoints = Header.NumPoints;
    InAsset->NumberOfSamples = Header.NumSamples;

    // Set up the Attribute and SpecialAttributeIndexes arrays in the asset,
    // expanding attributes with size > 1
    // If time was not an attribute in the file, we add it as an attribute
    // but we always set time to the frame's time, irrespective of the existence of a time
    // attribute in the file
    uint32 NumAttributesPerFileSample = 0;
    ParseAttributes(InAsset, Header, NumAttributesPerFileSample);

    // Get Age attribute index, we'll use this to ensure we sort point spawn time correctly
    int32 IDAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::POINTID);
	int32 AgeAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::AGE);
    int32 LifeAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::LIFE);
    int32 TypeAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::TYPE);
    int32 TimeAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::TIME);

    // Get references to the various data arrays of the asset and initialize them
    TArray<float> &FloatSampleData = InAsset->GetFloatSampleData();
    TArray<float> &SpawnTimes = InAsset->GetSpawnTimes();
    TArray<float> &LifeValues = InAsset->GetLifeValues();
    TArray<int32> &PointTypes = InAsset->GetPointTypes();
    TArray<FPointIndexes> &PointValueIndexes = InAsset->GetPointValueIndexes();

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

	// Due to the way that some of the DI functions work,
	// we expect that the point IDs start at zero, and increment as the points are spawned
	// Make sure this is the case by converting the point IDs as we read them
	int32 NextPointID = 0;
	TMap<int32, int32> HoudiniIDToNiagaraIDMap;

    // Expect cache_data key, object start, frames key
    if (!ReadNonContainerValue(Reader, Buffer, ObjectKey, false) || ObjectKey != TEXT("cache_data"))
        return false;

    if (!ReadMarker(Reader, Buffer, Marker) || Marker != MarkerObjectStart)
        return false;

    if (!ReadNonContainerValue(Reader, Buffer, ObjectKey, false) || ObjectKey != TEXT("frames"))
        return false;
    
    // Read the frames array, each entry contains a 'frame' object
    // Expect array start
    if (!ReadMarker(Reader, Buffer, Marker) || Marker != MarkerArrayStart)
        return false;

    uint32 FrameStartSampleIndex = 0;
    TArray<TArray<float>> TempFrameData;
    while (Buffer[0] != MarkerArrayEnd && !Reader.AtEnd())
    {
        // Expect array end (of frames array) or object start
        if (!ReadMarker(Reader, Buffer, Marker))
            return false;
        
        if (Marker == MarkerArrayEnd)
            break;
        
        // If not end of frames array, expect object start
        if (Marker != MarkerObjectStart)
            return false;

        float FrameNumber = 0.0f;
        float Time = 0;
        uint32 NumPointsInFrame = 0;

        // Read 'number' (frame number)
        if (!ReadNonContainerValue(Reader, Buffer, ObjectKey, false) || ObjectKey != TEXT("number"))
            return false;
        if (!ReadNonContainerValue(Reader, Buffer, FrameNumber))
            return false;

        // Read 'time'
        if (!ReadNonContainerValue(Reader, Buffer, ObjectKey, false) || ObjectKey != TEXT("time"))
            return false;
        if (!ReadNonContainerValue(Reader, Buffer, Time))
            return false;

        // Read 'num_points' (number of points in frame)
        if (!ReadNonContainerValue(Reader, Buffer, ObjectKey, false) || ObjectKey != TEXT("num_points"))
            return false;
        if (!ReadNonContainerValue(Reader, Buffer, NumPointsInFrame))
            return false;

        // Expect 'frame_data' key
        if (!ReadNonContainerValue(Reader, Buffer, ObjectKey, false) || ObjectKey != TEXT("frame_data"))
            return false;

        // Expect frame_start marker
        if (!ReadMarker(Reader, Buffer, Marker) || Marker != MarkerArrayStart)
            return false;

        // Ensure we have enough space in our FrameData array to read the samples for this frame
        TempFrameData.SetNum(NumPointsInFrame);
        float PreviousAge = 0.0f;
        bool bNeedToSort = false;
        for (uint32 SampleIndex = 0; SampleIndex < NumPointsInFrame; ++SampleIndex)
        {
            // Initialize attributes for this sample
            TempFrameData[SampleIndex].Init(0, NumAttributesPerFileSample);

            // Expect frame_start marker
            if (!ReadMarker(Reader, Buffer, Marker) || Marker != MarkerArrayStart)
                return false;

            for (uint32 AttrIndex = 0; AttrIndex < NumAttributesPerFileSample; ++AttrIndex)
            {
                float& Value = TempFrameData[SampleIndex][AttrIndex];
                // Read a float
                if (!ReadNonContainerValue(Reader, Buffer, Value))
                    return false;

                if (AgeAttributeIndex != INDEX_NONE && AttrIndex == AgeAttributeIndex)
                {
                    if (SampleIndex == 0)
                    {
                        PreviousAge = Value;
                    }
                    else if (PreviousAge < Value)
                    {
                        bNeedToSort = true;
                    }
                }
            }

            // Expect frame_end marker
            if (!ReadMarker(Reader, Buffer, Marker) || Marker != MarkerArrayEnd)
                return false;
        }

        // Sort this frame's data by age
        if (bNeedToSort)
        {
            TempFrameData.Sort<FHoudiniPointCacheSortPredicate>(FHoudiniPointCacheSortPredicate(INDEX_NONE, AgeAttributeIndex, IDAttributeIndex));
        }

        // Copy the frame data into the FloatSampleData array, determine unique points IDs
        // Also calculate SpawnTimes, LifeValues (if the life attribute exists)
        int32 CurrentID = -1;
        for (uint32 FrameSampleIndex = 0; FrameSampleIndex < NumPointsInFrame; ++FrameSampleIndex)
        {
            uint32 SampleIndex = FrameStartSampleIndex + FrameSampleIndex;
            for (uint32 AttrIndex = 0; AttrIndex < NumAttributesPerFileSample; ++AttrIndex)
            {
                // Get the float value for the attribute
                float FloatValue = TempFrameData[FrameSampleIndex][AttrIndex];

                // Handle point IDs here
                if (AttrIndex == IDAttributeIndex)
                {
                    // If the point ID doesn't exist in the Houdini/Niagara mapping, create a new a entry.
                    // Otherwise, replace the point ID with the Niagara ID.
                    int32 PointID = FMath::FloorToInt(FloatValue);

                    // The point ID may need to be replaced
                    if (!HoudiniIDToNiagaraIDMap.Contains(PointID))
                    {
                        // We found a new point, so we add it to the ID map
                        HoudiniIDToNiagaraIDMap.Add(PointID, NextPointID++);

                        // // Add a new array for that point's indexes
                        // PointValueIndexes.Add(FPointIndexes());
                    }

                    // Get the Niagara ID from the Houdini ID
                    CurrentID = HoudiniIDToNiagaraIDMap[PointID];
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
                FloatSampleData[SampleIndex + (TimeAttributeIndex * InAsset->NumberOfSamples)] = Time;
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
                // CurrentID = HoudiniIDToNiagaraIDMap[CurrentID];
            }
            else
            {
                CurrentID = SampleIndex;
            }
            
            // The time value comes from the frame entry
            const float CurrentTime = Time;

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

        // Expect frame_end marker
        if (!ReadMarker(Reader, Buffer, Marker) || Marker != MarkerArrayEnd)
            return false;

        // Expect object end
        if (!ReadMarker(Reader, Buffer, Marker) || Marker != MarkerObjectEnd)
            return false;

        FrameStartSampleIndex += NumPointsInFrame;
    }

    // Expect object end - cache_data
    if (!ReadMarker(Reader, Buffer, Marker) || Marker != MarkerObjectEnd)
        return false;

    // Expect object end - root
    if (!ReadMarker(Reader, Buffer, Marker) || Marker != MarkerObjectEnd)
        return false;

    return true;
}

bool FHoudiniPointCacheLoaderJSON::ReadMarker(FArchive &InReader, TArray<uint8> &InBuffer, TCHAR &OutMarker) const
{
    // Check that we are not at the end of the archive
    if (InReader.AtEnd())
        return false;
    // Read TYPE (1 byte)
    InReader.Serialize(InBuffer.GetData(), 1);
    OutMarker = InBuffer[0];
    return true;
}

bool FHoudiniPointCacheLoaderJSON::ReadHeader(FArchive &InReader, TArray<uint8> &InBuffer, FHoudiniPointCacheJSONHeader &OutHeader) const
{
    // Expect object start
    TCHAR Marker = '\0';
    if (!ReadMarker(InReader, InBuffer, Marker) || Marker != MarkerObjectStart)
        return false;

    FString HeaderKey;
    if (!ReadNonContainerValue(InReader, InBuffer, HeaderKey, false) || HeaderKey != TEXT("version"))
        return false;
    if (!ReadNonContainerValue(InReader, InBuffer, OutHeader.Version))
        return false;

    if (!ReadNonContainerValue(InReader, InBuffer, HeaderKey, false) || HeaderKey != TEXT("num_samples"))
        return false;
    if (!ReadNonContainerValue(InReader, InBuffer, OutHeader.NumSamples))
        return false;

    if (!ReadNonContainerValue(InReader, InBuffer, HeaderKey, false) || HeaderKey != TEXT("num_frames"))
        return false;
    if (!ReadNonContainerValue(InReader, InBuffer, OutHeader.NumFrames))
        return false;

    if (!ReadNonContainerValue(InReader, InBuffer, HeaderKey, false) || HeaderKey != TEXT("num_points"))
        return false;
    if (!ReadNonContainerValue(InReader, InBuffer, OutHeader.NumPoints))
        return false;

    if (!ReadNonContainerValue(InReader, InBuffer, HeaderKey, false) || HeaderKey != TEXT("num_attrib"))
        return false;
    if (!ReadNonContainerValue(InReader, InBuffer, OutHeader.NumAttributes))
        return false;

    // Preallocate Attribute arrays from NumAttributes
    OutHeader.Attributes.Empty(OutHeader.NumAttributes);
    OutHeader.AttributeSizes.Empty(OutHeader.NumAttributes);

    if (!ReadNonContainerValue(InReader, InBuffer, HeaderKey, false) || HeaderKey != TEXT("attrib_name"))
        return false;
    if (!ReadArray(InReader, InBuffer, OutHeader.Attributes))
        return false;

    if (!ReadNonContainerValue(InReader, InBuffer, HeaderKey, false) || HeaderKey != TEXT("attrib_size"))
        return false;
    if (!ReadArray(InReader, InBuffer, OutHeader.AttributeSizes))
        return false;

    if (!ReadNonContainerValue(InReader, InBuffer, HeaderKey, false) || HeaderKey != TEXT("data_type"))
        return false;
    if (!ReadNonContainerValue(InReader, InBuffer, OutHeader.DataType))
        return false;

    // Expect object end
    if (!ReadMarker(InReader, InBuffer, Marker) || Marker != MarkerObjectEnd)
        return false;
    return true;
}

bool FHoudiniPointCacheLoaderJSON::ReadNonContainerValue(FArchive &InReader, TArray<uint8> &InBuffer, FString &OutValue, bool bInReadMarkerType, TCHAR InMarkerType) const
{
    uint32 Size = 0;
    TCHAR MarkerType = '\0';
    if (bInReadMarkerType)
    {
        // Read TYPE (1 byte)
        InReader.Serialize(InBuffer.GetData(), 1);
        MarkerType = InBuffer[0];
    }
    else
    {
        MarkerType = InMarkerType;
    }
    
    if (MarkerType != MarkerTypeString && MarkerType != MarkerTypeChar)
    {
        UE_LOG(LogHoudiniNiagara, Error, TEXT("Expected string or char, found type %c"), MarkerType);
        return false;
    }

    if (MarkerType == MarkerTypeString)
    {
        // Get the number of characters in the string as Size
        if (!ReadNonContainerValue(InReader, InBuffer, Size))
        {
            UE_LOG(LogHoudiniNiagara, Error, TEXT("Could not determine string size."));
            return false;
        }
        // Strings are encoded with utf-8, calculate number of bytes to read
        Size = Size * sizeof(UTF8CHAR);
    }
    else
    {
        // char 1 byte
        Size = 1;
    }
    InReader.Serialize(InBuffer.GetData(), Size);
    InBuffer[Size] = '\0';
    OutValue = UTF8_TO_TCHAR(InBuffer.GetData());
    
    return true;
}

bool FHoudiniPointCacheLoaderJSON::ParseAttributes(UHoudiniPointCache *InAsset, const FHoudiniPointCacheJSONHeader &InHeader, uint32 &OutNumAttributesPerFileSample)
{
    // Get the relevant point cache asset data arrays
    TArray<FString> &AttributeArray = InAsset->AttributeArray;
    TArray<int32> &SpecialAttributeIndexes = InAsset->GetSpecialAttributeIndexes();

	// Reset the indexes of the special attributes
	SpecialAttributeIndexes.Init(INDEX_NONE, EHoudiniAttributes::HOUDINI_ATTR_SIZE);

    // Determine the number of attributes: this is the number of attributes as listed in
    // the header, expanded by their size
    uint32 NumAttributes = 0;
    for (uint32 HeaderAttrIndex = 0; HeaderAttrIndex < InHeader.NumAttributes; ++HeaderAttrIndex)
    {
        NumAttributes += InHeader.AttributeSizes[HeaderAttrIndex];
    }
    InAsset->NumberOfAttributes = NumAttributes;
    InAsset->AttributeArray.Empty(NumAttributes);

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

    OutNumAttributesPerFileSample = AttributeArray.Num();

    // If there was no time attribute in the file, add it
    if (!InAsset->IsValidAttributeAttributeIndex(EHoudiniAttributes::TIME))
    {
        InAsset->AttributeArray.Add(TEXT("time"));
        InAsset->NumberOfAttributes = InAsset->AttributeArray.Num();
        SpecialAttributeIndexes[EHoudiniAttributes::TIME] = InAsset->NumberOfAttributes - 1;
    }

	return true;
}