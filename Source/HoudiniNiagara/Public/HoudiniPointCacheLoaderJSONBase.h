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

#pragma once

#include "CoreMinimal.h"
#include "HoudiniPointCacheLoader.h"

class UHoudiniPointCache;

struct FHoudiniPointCacheJSONHeader 
{
    FString Version;
    uint32 NumSamples;
    uint32 NumFrames;
    uint32 NumPoints;
    uint32 NumAttributes;
    uint32 NumAttributeComponents;
    TArray<FString> Attributes;
    TArray<uint8> AttributeSizes;
    TArray<unsigned char> AttributeComponentDataTypes;
    FString DataType;
};


/**
 * Base class for JSON-based Houdini Point Cache loaders.
 */
class FHoudiniPointCacheLoaderJSONBase : public FHoudiniPointCacheLoader
{
    public:
        /** Construct with the path to the file */
        FHoudiniPointCacheLoaderJSONBase(const FString& InFilePath);

        virtual ~FHoudiniPointCacheLoaderJSONBase();

    protected:
        /** Using InHeader, parse the attributes and generate the expanded list (P -> P.x, P.y, P.z etc) and initialize (pre-allocate/reset) the
         * internal arrays of InAsset, such as FloatSampleData, SpawnTimes etc.
         */
        virtual bool ParseAttributesAndInitAsset(UHoudiniPointCache *InAsset, const struct FHoudiniPointCacheJSONHeader &InHeader);

        /** Process one frame's data (InFrameData).
         * @param InAsset The point cache asset to populate.
         * @param InFrameNumber The frame number
         * @param InFrameData The frame's data, an array of arrays (point and attribute values for the point).
         * @param InFrameTime The time, in seconds, of the frame.
         * @param InFrameStartSampleIndex The sample index of the first sample in the frame.
         * @param InNumPointsInFrame The number of points in this frame.
         * @param InNumAttributesPerPoint The number of attributes in a point's sample.
         * @param InHeader The header data of the point cache file.
         * @param InHoudiniIDToNiagaraIDMap A map of point ids from the file, to our internal id
         * @param OutNextPointID The next point id (incremented everytime a new point is detected).
         * @return false if processing the frame failed.
         */
        virtual bool ProcessFrame(UHoudiniPointCache *InAsset, float InFrameNumber, const TArray<TArray<float>> &InFrameData, float InFrameTime, uint32 InFrameStartSampleIndex, uint32 InNumPointsInFrame, uint32 InNumAttributesPerPoint, const FHoudiniPointCacheJSONHeader &InHeader, TMap<int32, int32>& InHoudiniIDToNiagaraIDMap, int32 &OutNextPointID) const;

};
