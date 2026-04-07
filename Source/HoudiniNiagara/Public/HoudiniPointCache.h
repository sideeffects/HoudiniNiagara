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
#include "Containers/Array.h"
#include "Containers/ContainersFwd.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CoreMiscDefines.h" 
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderResource.h"
#include "RHIDefinitions.h"
#include "RHIUtilities.h"
#include "Runtime/Launch/Resources/Version.h"
#include "ShaderCompiler.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
#include "UObject/AssetRegistryTagsContext.h"
#endif

#include "HoudiniPointCache.generated.h"

DECLARE_LOG_CATEGORY_EXTERN( LogHoudiniNiagara, All, All );

UENUM()
enum EHoudiniAttributes
{
	HOUDINI_ATTR_BEGIN,

	POSITION = HOUDINI_ATTR_BEGIN,
	NORMAL,
	TIME,
	POINTID,
	LIFE,
	COLOR,
	ALPHA,
	VELOCITY,
	TYPE,
	IMPULSE,
	AGE,

	HOUDINI_ATTR_SIZE,
	HOUDINI_ATTR_END = HOUDINI_ATTR_SIZE - 1
	
};

USTRUCT()
struct FPointIndexes
{
	GENERATED_BODY()

	// Simple structure for storing all the sample indexes used for a given point
	UPROPERTY()
	TArray<int64> SampleIndexes;

	friend FArchive& operator<<(FArchive& Ar, FPointIndexes& PointIndexes);
	bool Serialize(FArchive& Ar);
};

UENUM()
enum class EHoudiniPointCacheFileType : uint8
{
	Invalid,
	CSV,
	JSON,
	BJSON,
};

struct FNiagaraDIHoudini_StaticDataPassToRT
{
	~FNiagaraDIHoudini_StaticDataPassToRT()
	{
		//UE_LOG(LogHoudiniNiagara, Warning, TEXT("Deleted!"));
	}

	TArray64<float> FloatData;
	TArray64<float> SpawnTimes;
	TArray64<float> LifeValues;
	TArray64<int32> PointTypes;
	TArray<int32> SpecialAttributeIndexes;
	TArray64<int32> PointValueIndexes;
	TArray<FString> Attributes;

	int64 NumSamples;
	int32 NumAttributes;
	int64 NumPoints;
	int64 MaxNumIndexesPerPoint;
};

/**
 * point cache resource.
 */
class  FHoudiniPointCacheResource : public FRenderResource
{
public:

	// GPU Buffers
	FRWBuffer FloatValuesGPUBuffer;
	FRWBuffer SpecialAttributeIndexesGPUBuffer;
	FRWBuffer SpawnTimesGPUBuffer;
	FRWBuffer LifeValuesGPUBuffer;
	FRWBuffer PointTypesGPUBuffer;
	FRWBuffer PointValueIndexesGPUBuffer;

	int64 MaxNumberOfIndexesPerPoint;
	int64 NumSamples;
	int32 NumAttributes;
	int64 NumPoints;

	TArray<FString> Attributes;

	TUniquePtr<struct FNiagaraDIHoudini_StaticDataPassToRT> CachedData;

	/** Default constructor. */
	FHoudiniPointCacheResource() : CachedData(nullptr){}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
#else
	virtual void InitRHI() override;
#endif
	virtual void ReleaseRHI() override;

	virtual FString GetFriendlyName() const override { return TEXT("FHoudiniPointCacheResource"); }

	void AcceptStaticDataUpdate(TUniquePtr<struct FNiagaraDIHoudini_StaticDataPassToRT>& Update);

	virtual ~FHoudiniPointCacheResource() {}
};


UCLASS(BlueprintType)
class HOUDININIAGARA_API UHoudiniPointCache : public UObject
{
	friend class UNiagaraDataInterfaceHoudini;

    GENERATED_UCLASS_BODY()
 
    public:

	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	
	//-----------------------------------------------------------------------------------------
	//  MEMBER FUNCTIONS
	//-----------------------------------------------------------------------------------------

#if WITH_EDITOR
	bool UpdateFromFile( const FString& TheFileName );
#endif

	void SetFileName( const FString& TheFilename );

	// Returns the number of points found in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	int64 GetNumberOfPoints() const;

	// Returns the number of samples found in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	int64 GetNumberOfSamples() const;

	// Returns the number of attributes found in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	int32 GetNumberOfAttributes() const;

	// Return the attribute index for a specific attribute
	//UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	int32 GetAttributeAttributeIndex(const EHoudiniAttributes& Attr) const;

	// Returns if the specific attribute has a valid attribute index
	bool IsValidAttributeAttributeIndex(const EHoudiniAttributes& Attr) const;

	// Returns the attribute index for a given string. 
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetAttributeIndexFromString(const FString& Attribute, int32& AttributeIndex) const;

	// Returns the attribute index for a given string. This is a static version of the function that
	// takes the attribute name array as an argument as well.
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	static bool GetAttributeIndexInArrayFromString(const FString& InAttribute, const TArray<FString>& InAttributeArray, int32& OutAttributeIndex);

	// Returns the float value at a given point in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetFloatValue( const int64& sampleIndex, const int32& attrIndex, float& value ) const;
	// Returns the float value at a given point in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetFloatValueForString( const int64& sampleIndex, const FString& Attribute, float& value ) const;
	/*
	// Returns the string value at a given point in the point cache
	bool GetCSVStringValue( const int64& sampleIndex, const int32& attrIndex, FString& value );
	// Returns the string value at a given point in the point cache
	bool GetCSVStringValue( const int64& sampleIndex, const FString& Attribute, FString& value );
	*/
	// Returns a Vector3 for a given point in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetVectorValue( const int64& sampleIndex, const int32& attrIndex, FVector& value, const bool& DoSwap = true, const bool& DoScale = true ) const;
	// Returns a Vector3 for a given point in the point cache by column name
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetVectorValueForString(const int64& sampleIndex, const FString& Attribute, FVector& value, const bool& DoSwap = true, const bool& DoScale = true) const;
	// Returns a Vector4 for a given point in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetVector4Value( const int64& sampleIndex, const int32& attrIndex, FVector4& value ) const;
	// Returns a Vector4 for a given point in the point cache by column name
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetVector4ValueForString(const int64& sampleIndex, const FString& Attribute, FVector4& value ) const;
	// Returns a Quat for a given point in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetQuatValue( const int64& sampleIndex, const int32& attrIndex, FQuat& value, const bool& DoHoudiniToUnrealConversion = true ) const;
	// Returns a Quat for a given point in the point cache by column name
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetQuatValueForString(const int64& sampleIndex, const FString& Attribute, FQuat& value, const bool& DoHoudiniToUnrealConversion = true ) const;

	// Returns a time value for a given point in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetTimeValue( const int64& sampleIndex, float& value ) const;
	// Returns a Position Vector3 for a given point in the point cache (converted to unreal's coordinate system)
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPositionValue( const int64& sampleIndex, FVector& value ) const;
	// Returns a Normal Vector3 for a given point in the point cache (converted to unreal's coordinate system)
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetNormalValue( const int64& sampleIndex, FVector& value ) const;
	// Returns a Color for a given point in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetColorValue( const int64& sampleIndex, FLinearColor& value ) const;
	// Returns a Velocity Vector3 for a given point in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetVelocityValue(const int64& sampleIndex, FVector& value ) const;
	// Returns an Impulse float value for a given point in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetImpulseValue(const int64& sampleIndex, float& value) const;

	// Get the last sample index for a given time value (the sample with a time smaller or equal to desiredTime)
	// If the point cache doesn't have time informations, returns false and set LastsampleIndex to the last sample in the file
	// If desiredTime is smaller than the time value in the first sample, LastsampleIndex will be set to -1
	// If desiredTime is higher than the last time value in the last sample of the point cache, LastIndex will be set to the last sample's index
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetLastSampleIndexAtTime( const float& desiredTime, int64& lastSampleIndex ) const;

	// Get the last pointID of the points to be spawned at time t
	// Invalid Index are used to indicate edge cases:
	// -1 will be returned if there is no points to spawn ( t is smaller than the first point time )
	// NumberOfSamples will be returned if all points in the CSV have been spawned ( t is higher than the last point time )
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetLastPointIDToSpawnAtTime( const float& time, int64& lastID ) const;
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointIDsToSpawnAtTime(
		const float& desiredTime,
		int64& MinID, int64& MaxID, int64& Count,
		int64& LastSpawnedPointID, float& LastSpawnTime, float& LastSpawnTimeRequest) const;

	bool GetPointIDsToSpawnAtTime_DEPR(
		const float& desiredTime,
		int64& MinID, int64& MaxID, int64& Count,
		int64& LastSpawnedPointID, float& LastSpawnTime ) const;

	// Returns the previous and next sample indexes for reading the values of a specified point at a given time
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetSampleIndexesForPointAtTime(const int64& PointID, const float& desiredTime, int64& PrevSampleIndex, int64& NextSampleIndex, float& PrevWeight) const;
	// Returns the value for a point at a given time value (linearly interpolated)
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointValueAtTime(const int64& PointID, const int32& AttributeIndex, const float& desiredTime, float& Value) const;
	// Returns the value for a point at a given time value (linearly interpolated), via the attribute name
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointValueAtTimeForString(const int64& PointID, const FString& Attribute, const float& desiredTime, float& Value) const;
	
	// Returns the Vector Value for a given point at a given time value (linearly interpolated)
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointVectorValueAtTime(int64 PointID, int32 AttributeIndex, float desiredTime, FVector& Vector, bool DoSwap, bool DoScale) const;
	
	// Returns the Vector Value for a given point at a given time value (linearly interpolated), via the attribute name
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointVectorValueAtTimeForString(int64 PointID, const FString& Attribute, float desiredTime, FVector& Vector, bool DoSwap, bool DoScale) const;

	// Returns the Vector4 Value for a given point at a given time value (linearly interpolated)
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointVector4ValueAtTime(int64 PointID, int32 AttributeIndex, float desiredTime, FVector4& Vector) const;
	
	// Returns the Vector4 Value for a given point at a given time value (linearly interpolated), via the attribute name
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointVector4ValueAtTimeForString(int64 PointID, const FString& Attribute, float desiredTime, FVector4& Vector) const;

	// Returns the Quat Value for a given point at a given time value (linearly interpolated)
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointQuatValueAtTime(int64 PointID, int32 AttributeIndex, float desiredTime, FQuat& Quat, bool DoHoudiniToUnrealConversion = true ) const;
	
	// Returns the Quat Value for a given point at a given time value (linearly interpolated), via the attribute name
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointQuatValueAtTimeForString(int64 PointID, const FString& Attribute, float desiredTime, FQuat& Quat, bool DoHoudiniToUnrealConversion = true ) const;

	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointFloatValueAtTime(int64 PointID, int32 AttributeIndex, float desiredTime, float& Value) const;

	// Return the integer value of the point at the keyframe before the desired time. No value interpolation will take place.
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointInt32ValueAtTime(int64 PointID, int32 AttributeIndex, float desiredTime, int32& Value) const;
	
	// Returns the Position Value for a given point at a given time value (linearly interpolated)
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointPositionAtTime(const int64& PointID, const float& desiredTime, FVector& Vector) const;
	// Return a given point's life value at spawn
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointLife(const int64& PointID, float& Value) const;
	// Return a point's life for a given time value
	// Note this function currently behaves exactly the same as GetPointLife
	// since the Lifetime value is currently treated as a constant. This could
	// change in the future.
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointLifeAtTime(const int64& PointID, const float& DesiredTime, float& Value) const;
	// Return a point's type at spawn
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointType(const int64& PointID, int32& Value) const;


	// Returns the maximum number of indexes per point, used for flattening the buffer for HLSL conversion
	int32 GetMaxNumberOfPointValueIndexes() const;

	//-----------------------------------------------------------------------------------------
	//  MEMBER VARIABLES
	//-----------------------------------------------------------------------------------------

	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties" )
	FString FileName;

	// The number of values stored in the point cache
	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties" )
	int64 NumberOfSamples;

	// The number of attributes stored in the point cache
	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties" )
	int32 NumberOfAttributes;

	// The number of unique points found in the point cache
	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties")
	int64 NumberOfPoints;

	// The number of frames imported into the point cache
	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties")
	int32 NumberOfFrames;

	// The first frame of the exported frame range
	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties")
	float FirstFrame;

	// The last frame of the exported frame range
	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties")
	float LastFrame;

	// The minimum sample time value, in seconds, in the point cache
	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties")
	float MinSampleTime;

	// The maximum sample time value, in seconds, in the point cache
	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties")
	float MaxSampleTime;

	// The source title row for CSV files, describing the content of each column and used to locate specific values in the point cache.
	// Editing this will trigger a re-import of the point cache.
	UPROPERTY(EditAnywhere, Category = "Houdini Point Cache Properties")
	FString SourceCSVTitleRow;

	// The final attribute names used by the asset after parsing.
	// Packed vector values are expanded, so additional attributes (.0, .1, ... or .x, .y, .z) might have been inserted.
	// Use the indexes in this array to access your data.
	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties" )
	TArray<FString> AttributeArray;

#if WITH_EDITORONLY_DATA
	/** Importing data and options used for this asset */
	UPROPERTY( EditAnywhere, Instanced, Category = ImportSettings )
	TObjectPtr<class UAssetImportData> AssetImportData;

	// Raw data of the source file so that we can export it again.
	UPROPERTY()
	TArray<uint8> RawDataCompressed;

	// Compression scheme used to compress raw 
	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties" )
	FName RawDataFormatID;

	// Size of data when uncompressed
	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties" )
	uint64 RawDataUncompressedSize;

	// Compression scheme used to compress raw 
	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties" )
	FName RawDataCompressionMethod;

#endif
	
	// Indicates that we're using a large file
	// Source Data is stored in BulkRawData
	UPROPERTY()
	bool bLargeFile;

#if WITH_EDITOR
	bool HasRawData() const { return bLargeFile ? RawDataCompressed.Num() > 0 : false; };

	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent & PropertyChangedEvent) override;
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
#endif

	virtual void GetAssetRegistryTags(TArray< FAssetRegistryTag > & OutTags) const override;
	
	void BeginDestroy() override;

	// Data Accessors, const and non-const versions
	TArray<float, FDefaultAllocator64>& GetFloatSampleData() { return FloatSampleData64; }

	const TArray<float, FDefaultAllocator64>& GetFloatSampleData() const { return FloatSampleData64; }

	TArray<float, FDefaultAllocator64>& GetSpawnTimes() { return SpawnTimes64; }

	const TArray<float, FDefaultAllocator64>& GetSpawnTimes() const { return SpawnTimes64; }

	TArray<float, FDefaultAllocator64>& GetLifeValues() { return LifeValues64; }

	const TArray<float, FDefaultAllocator64>& GetLifeValues() const { return LifeValues64; }

	TArray<int32, FDefaultAllocator64>& GetPointTypes() { return PointTypes64; }

	const TArray<int32, FDefaultAllocator64>& GetPointTypes() const { return PointTypes64; }

	TArray<int32>& GetSpecialAttributeIndexes() { return SpecialAttributeIndexes; }

	const TArray<int32>& GetSpecialAttributeIndexes() const { return SpecialAttributeIndexes; }

	TArray<FPointIndexes, FDefaultAllocator64>& GetPointValueIndexes() { return PointValueIndexes64; }

	const TArray<FPointIndexes, FDefaultAllocator64>& GetPointValueIndexes() const { return PointValueIndexes64; }

	UFUNCTION(BlueprintCallable, Category = "Houdini Point Cache Settings")
	bool GetUseCustomCSVTitleRow() const { return UseCustomCSVTitleRow; }

	UFUNCTION(BlueprintCallable, Category = "Houdini Point Cache Settings")
	void SetUseCustomCSVTitleRow(bool bInUseCustomCSVTitleRow) { UseCustomCSVTitleRow = bInUseCustomCSVTitleRow; }

	/** The GPU resource for this point cache. */
	TUniquePtr<class FHoudiniPointCacheResource> Resource;

	void RequestPushToGPU();

	private:


	// Array containing all the sample data converted to floats
	UPROPERTY()
	TArray<float> FloatSampleData_DEPRECATED;
	
	// Array containing the spawn times for each point in the point cache
	UPROPERTY()
	TArray<float> SpawnTimes_DEPRECATED;

	// Array containing all the life values for each point in the point cache
	UPROPERTY()
	TArray<float> LifeValues_DEPRECATED;

	// Array containing all the type values for each point in the point cache
	UPROPERTY()
	TArray<int32> PointTypes_DEPRECATED;

	// Array containing the column indexes of the special attributes
	UPROPERTY()
	TArray<int32> SpecialAttributeIndexes;

	// Sample indexes for each point
	UPROPERTY()
	TArray<FPointIndexes> PointValueIndexes_DEPRECATED;

	/** For CSV source files, whether to use a custom title row. */
	UPROPERTY()
	bool UseCustomCSVTitleRow;

	// The type of source file, such as CSV or JSON.
	UPROPERTY()
	EHoudiniPointCacheFileType FileType;


	// Array containing all the sample data converted to floats
	TArray<float, FDefaultAllocator64> FloatSampleData64;
	
	// Array containing the spawn times for each point in the point cache
	TArray<float, FDefaultAllocator64> SpawnTimes64;

	// Array containing all the life values for each point in the point cache
	TArray<float, FDefaultAllocator64> LifeValues64;

	// Array containing all the type values for each point in the point cache
	TArray<int32, FDefaultAllocator64> PointTypes64;

	// Sample indexes for each point
	TArray<FPointIndexes, FDefaultAllocator64> PointValueIndexes64;
};