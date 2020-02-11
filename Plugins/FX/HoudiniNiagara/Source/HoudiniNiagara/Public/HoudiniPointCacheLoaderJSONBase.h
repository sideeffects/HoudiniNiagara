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
