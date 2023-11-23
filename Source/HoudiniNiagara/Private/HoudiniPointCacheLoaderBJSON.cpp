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

#include "HoudiniPointCacheLoaderBJSON.h"

#include "HoudiniPointCache.h"

#include "CoreMinimal.h"
#include "Misc/CoreMiscDefines.h" 
#include "Serialization/MemoryReader.h"
#include "ShaderCompiler.h"


const unsigned char FHoudiniPointCacheLoaderBJSON::MarkerTypeChar;
const unsigned char FHoudiniPointCacheLoaderBJSON::MarkerTypeInt8;
const unsigned char FHoudiniPointCacheLoaderBJSON::MarkerTypeUInt8;
const unsigned char FHoudiniPointCacheLoaderBJSON::MarkerTypeBool;
const unsigned char FHoudiniPointCacheLoaderBJSON::MarkerTypeInt16;
const unsigned char FHoudiniPointCacheLoaderBJSON::MarkerTypeUInt16;
const unsigned char FHoudiniPointCacheLoaderBJSON::MarkerTypeInt32;
const unsigned char FHoudiniPointCacheLoaderBJSON::MarkerTypeUInt32;
const unsigned char FHoudiniPointCacheLoaderBJSON::MarkerTypeInt64;
const unsigned char FHoudiniPointCacheLoaderBJSON::MarkerTypeUInt64;
const unsigned char FHoudiniPointCacheLoaderBJSON::MarkerTypeFloat32;
const unsigned char FHoudiniPointCacheLoaderBJSON::MarkerTypeFloat64;
const unsigned char FHoudiniPointCacheLoaderBJSON::MarkerTypeString;
const unsigned char FHoudiniPointCacheLoaderBJSON::MarkerObjectStart;
const unsigned char FHoudiniPointCacheLoaderBJSON::MarkerObjectEnd;
const unsigned char FHoudiniPointCacheLoaderBJSON::MarkerArrayStart;
const unsigned char FHoudiniPointCacheLoaderBJSON::MarkerArrayEnd;

FHoudiniPointCacheLoaderBJSON::FHoudiniPointCacheLoaderBJSON(const FString& InFilePath) :
    FHoudiniPointCacheLoaderJSONBase(InFilePath)
{

}

#if WITH_EDITOR
bool FHoudiniPointCacheLoaderBJSON::LoadToAsset(UHoudiniPointCache *InAsset)
{
    const FString& InFilePath = GetFilePath();
	FScopedLoadingState ScopedLoadingState(*InFilePath);

    // Reset the reader and load the whole file into raw buffer
    if (!LoadRawPointCacheData(InAsset, InFilePath))
    {
        UE_LOG(LogHoudiniNiagara, Warning, TEXT("Failed to read file '%s' error."), *InFilePath);
        return false;
    }

    // Pre-allocate and reset buffer
    Buffer.SetNumZeroed(1024);
    
    // Construct reader to read from the (currently) uncompressed data buffer in memory.
    Reader = MakeUnique<FMemoryReader>(InAsset->RawDataCompressed);
	if (!Reader)
	{
	    UE_LOG(LogHoudiniNiagara, Warning, TEXT("Failed to reader data from raw data buffer."));
		return false;
	}

    // Read start of root object
    unsigned char Marker = '\0';
    if (!ReadMarker(Marker) || Marker != MarkerObjectStart)
    {
        UE_LOG(LogHoudiniNiagara, Error, TEXT("Expected root object start."));
        return false;
    }
    
    FString ObjectKey;
    if (!ReadNonContainerValue(ObjectKey, false))
        return false;
    if (ObjectKey != TEXT("header"))
        return false;

    // Found header
    FHoudiniPointCacheJSONHeader Header;
    if (!ReadHeader(Header))
    {
        UE_LOG(LogHoudiniNiagara, Error, TEXT("Could not read header."));
        return false;
    }

    // Set up the Attribute and SpecialAttributeIndexes arrays in the asset,
    // expanding attributes with size > 1
    // If time was not an attribute in the file, we add it as an attribute
    // but we always set time to the frame's time, irrespective of the existence of a time
    // attribute in the file
    uint32 NumAttributesPerFileSample = Header.NumAttributeComponents;
    ParseAttributesAndInitAsset(InAsset, Header);

    // Get Age attribute index, we'll use this to ensure we sort point spawn time correctly
    int32 IDAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::POINTID);
	int32 AgeAttributeIndex = InAsset->GetAttributeAttributeIndex(EHoudiniAttributes::AGE);

	// Due to the way that some of the DI functions work,
	// we expect that the point IDs start at zero, and increment as the points are spawned
	// Make sure this is the case by converting the point IDs as we read them
	int32 NextPointID = 0;
	TMap<int32, int32> HoudiniIDToNiagaraIDMap;

    // Expect cache_data key, object start, frames key
    if (!ReadNonContainerValue(ObjectKey, false, MarkerTypeString) || ObjectKey != TEXT("cache_data"))
        return false;

    if (!ReadMarker(Marker) || Marker != MarkerObjectStart)
        return false;

    if (!ReadNonContainerValue(ObjectKey, false, MarkerTypeString) || ObjectKey != TEXT("frames"))
        return false;
    
    // Read the frames array, each entry contains a 'frame' object
    // Expect array start
    if (!ReadMarker(Marker) || Marker != MarkerArrayStart)
        return false;

    uint32 NumFramesRead = 0;
    uint32 FrameStartSampleIndex = 0;
    TArray<TArray<float>> TempFrameData;
    while (!Reader->AtEnd() && !IsNext(MarkerArrayEnd))
    {
        // Expect object start
        if (!ReadMarker(Marker) || Marker != MarkerObjectStart)
            return false;

        float FrameNumber = 0.0f;
        float Time = 0;
        uint32 NumPointsInFrame = 0;

        // Read 'number' (frame number)
        if (!ReadNonContainerValue(ObjectKey, false, MarkerTypeString) || ObjectKey != TEXT("number"))
            return false;
        if (!ReadNonContainerValue(FrameNumber, false, MarkerTypeUInt32))
            return false;

        // Read 'time'
        if (!ReadNonContainerValue(ObjectKey, false, MarkerTypeString) || ObjectKey != TEXT("time"))
            return false;
        if (!ReadNonContainerValue(Time, false, MarkerTypeFloat32))
            return false;

        // Read 'num_points' (number of points in frame)
        if (!ReadNonContainerValue(ObjectKey, false, MarkerTypeString) || ObjectKey != TEXT("num_points"))
            return false;
        if (!ReadNonContainerValue(NumPointsInFrame, false, MarkerTypeUInt32))
            return false;

        // Expect 'frame_data' key
        if (!ReadNonContainerValue(ObjectKey, false, MarkerTypeString) || ObjectKey != TEXT("frame_data"))
            return false;

        // Expect array start marker
        if (!ReadMarker(Marker) || Marker != MarkerArrayStart)
            return false;

        // Ensure we have enough space in our FrameData array to read the samples for this frame
        TempFrameData.SetNum(NumPointsInFrame);
        float PreviousAge = 0.0f;
        bool bNeedToSort = false;
        for (uint32 SampleIndex = 0; SampleIndex < NumPointsInFrame; ++SampleIndex)
        {
            // Initialize attributes for this sample
            TempFrameData[SampleIndex].Init(0, NumAttributesPerFileSample);

            // Expect array start marker
            if (!ReadMarker(Marker) || Marker != MarkerArrayStart)
                return false;

            for (uint32 AttrIndex = 0; AttrIndex < NumAttributesPerFileSample; ++AttrIndex)
            {
                float& Value = TempFrameData[SampleIndex][AttrIndex];
                // Read a value
                if (!ReadNonContainerValue(Value, false, Header.AttributeComponentDataTypes[AttrIndex]))
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

            // Expect array end marker
            if (!ReadMarker(Marker) || Marker != MarkerArrayEnd)
                return false;
        }

        // Sort this frame's data by age
        if (bNeedToSort)
        {
            TempFrameData.Sort<FHoudiniPointCacheSortPredicate>(FHoudiniPointCacheSortPredicate(INDEX_NONE, AgeAttributeIndex, IDAttributeIndex));
        }

        ProcessFrame(InAsset, FrameNumber, TempFrameData, Time, FrameStartSampleIndex, NumPointsInFrame, NumAttributesPerFileSample, Header, HoudiniIDToNiagaraIDMap, NextPointID);

        // Expect array end marker
        if (!ReadMarker(Marker) || Marker != MarkerArrayEnd)
            return false;

        // Expect object end
        if (!ReadMarker(Marker) || Marker != MarkerObjectEnd)
            return false;

        FrameStartSampleIndex += NumPointsInFrame;
        NumFramesRead++;
    }

    if (NumFramesRead != Header.NumFrames)
    {
        UE_LOG(LogHoudiniNiagara, Error, TEXT("Inconsistent num_frames in header vs body: %d vs %d"), Header.NumFrames, NumFramesRead);
        return false;
    }

    // Expect array end - frames
    if (!ReadMarker(Marker) || Marker != MarkerArrayEnd)
        return false;

    // Expect object end - cache_data
    if (!ReadMarker(Marker) || Marker != MarkerObjectEnd)
        return false;

    // Expect object end - root
    if (!ReadMarker(Marker) || Marker != MarkerObjectEnd)
        return false;

    // We have finished ingesting the data.
    // Finalize data loading by compressing raw data.
    CompressRawData(InAsset);

    return true;
}
#endif

bool FHoudiniPointCacheLoaderBJSON::ReadMarker(unsigned char &OutMarker)
{
    if (!CheckReader())
        return false;
        
    // Read TYPE (1 byte)
    Reader->Serialize(Buffer.GetData(), 1);
    OutMarker = Buffer[0];
    return true;
}

bool FHoudiniPointCacheLoaderBJSON::ReadHeader(FHoudiniPointCacheJSONHeader &OutHeader)
{
    // Expect object start
    unsigned char Marker = '\0';
    if (!ReadMarker(Marker) || Marker != MarkerObjectStart)
        return false;

    // Attempt to reach each key and value of the header, return false if any value is missing or
    // not of the expected type
    FString HeaderKey;
    if (!ReadNonContainerValue(HeaderKey, false, MarkerTypeString) || HeaderKey != TEXT("version"))
        return false;
    if (!ReadNonContainerValue(OutHeader.Version, false, MarkerTypeString))
        return false;

    if (!ReadNonContainerValue(HeaderKey, false, MarkerTypeString) || HeaderKey != TEXT("num_samples"))
        return false;
    if (!ReadNonContainerValue(OutHeader.NumSamples, false, MarkerTypeUInt32))
        return false;

    if (!ReadNonContainerValue(HeaderKey, false, MarkerTypeString) || HeaderKey != TEXT("num_frames"))
        return false;
    if (!ReadNonContainerValue(OutHeader.NumFrames, false, MarkerTypeUInt32))
        return false;

    if (!ReadNonContainerValue(HeaderKey, false, MarkerTypeString) || HeaderKey != TEXT("num_points"))
        return false;
    if (!ReadNonContainerValue(OutHeader.NumPoints, false, MarkerTypeUInt32))
        return false;

    if (!ReadNonContainerValue(HeaderKey, false, MarkerTypeString) || HeaderKey != TEXT("num_attrib"))
        return false;
    if (!ReadNonContainerValue(OutHeader.NumAttributes, false, MarkerTypeUInt16))
        return false;

    // Preallocate Attribute arrays from NumAttributes
    OutHeader.Attributes.Empty(OutHeader.NumAttributes);
    OutHeader.AttributeSizes.Empty(OutHeader.NumAttributes);

    if (!ReadNonContainerValue(HeaderKey, false, MarkerTypeString) || HeaderKey != TEXT("attrib_name"))
        return false;
    if (!ReadArray(OutHeader.Attributes, MarkerTypeString))
        return false;

    // Check that attrib_name was the expected size
    if (OutHeader.Attributes.Num() != OutHeader.NumAttributes)
    {
        UE_LOG(LogHoudiniNiagara, Error, TEXT("Header inconsistent: attrib_name array size mismatch: %d vs %d"), OutHeader.Attributes.Num(), OutHeader.NumAttributes);
        return false;
    }

    if (!ReadNonContainerValue(HeaderKey, false, MarkerTypeString) || HeaderKey != TEXT("attrib_size"))
        return false;
    if (!ReadArray(OutHeader.AttributeSizes, MarkerTypeUInt8))
        return false;

    // Check that attrib_size was the expected size
    if (OutHeader.AttributeSizes.Num() != OutHeader.NumAttributes)
    {
        UE_LOG(LogHoudiniNiagara, Error, TEXT("Header inconsistent: attrib_size array size mismatch: %d vs %d"), OutHeader.AttributeSizes.Num(), OutHeader.NumAttributes);
        return false;
    }

    // Calculate the number of attribute components (sum of attribute size over all attributes)
    OutHeader.NumAttributeComponents = 0;
    for (uint32 AttrSize : OutHeader.AttributeSizes)
    {
        OutHeader.NumAttributeComponents += AttrSize;
    }
    OutHeader.AttributeComponentDataTypes.Empty(OutHeader.NumAttributeComponents);

    if (!ReadNonContainerValue(HeaderKey, false, MarkerTypeString) || HeaderKey != TEXT("attrib_data_type"))
        return false;
    if (!ReadArray(OutHeader.AttributeComponentDataTypes, MarkerTypeChar))
        return false;

    // Check that attrib_data_type was the expected size
    if (OutHeader.AttributeComponentDataTypes.Num() != OutHeader.NumAttributeComponents)
    {
        UE_LOG(LogHoudiniNiagara, Error, TEXT("Header inconsistent: attrib_data_type array size mismatch: %d vs %d"), OutHeader.AttributeComponentDataTypes.Num(), OutHeader.NumAttributeComponents);
        return false;
    }

    if (!ReadNonContainerValue(HeaderKey, false, MarkerTypeString) || HeaderKey != TEXT("data_type"))
        return false;
    if (!ReadNonContainerValue(OutHeader.DataType, false, MarkerTypeString))
        return false;

    // Expect object end
    if (!ReadMarker(Marker) || Marker != MarkerObjectEnd)
        return false;
    return true;
}

bool FHoudiniPointCacheLoaderBJSON::ReadNonContainerValue(FString &OutValue, bool bInReadMarkerType, unsigned char InMarkerType)
{
    if (!CheckReader())
        return false;

    uint32 Size = 0;
    unsigned char MarkerType = '\0';
    if (bInReadMarkerType)
    {
        // Read TYPE (1 byte)
        Reader->Serialize(Buffer.GetData(), 1);
        MarkerType = Buffer[0];
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
        if (!ReadNonContainerValue(Size, true))
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
    Reader->Serialize(Buffer.GetData(), Size);
    if (Size >= static_cast<uint32>(Buffer.Num()))
    {
        UE_LOG(LogHoudiniNiagara, Error, TEXT("Buffer too small to read string with size %d"), Size)
        return false;
    }
    Buffer[Size] = '\0';
    OutValue = UTF8_TO_TCHAR(Buffer.GetData());
    
    return true;
}

bool FHoudiniPointCacheLoaderBJSON::Peek(int32 InSize) 
{
    if (!CheckReader())
        return false;

    int64 Position = Reader->Tell();
    if (Reader->TotalSize() - Position < InSize)
    {
        return false;
    }
    Reader->ByteOrderSerialize(Buffer.GetData(), InSize);
    Reader->Seek(Position);

    return true;
}

bool FHoudiniPointCacheLoaderBJSON::CheckReader(bool bInCheckAtEnd) const
{
    if (!Reader || !Reader.IsValid())
    {
        UE_LOG(LogHoudiniNiagara, Error, TEXT("Binary JSON reader is not valid."))
        return false;
    }
    // Check that we are not at the end of the archive
    if (bInCheckAtEnd && Reader->AtEnd())
    {
        UE_LOG(LogHoudiniNiagara, Error, TEXT("Binary JSON reader reach EOF early."))
        return false;
    }

    return true;
}
