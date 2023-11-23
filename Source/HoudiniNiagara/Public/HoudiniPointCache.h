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
#include "HAL/PlatformProcess.h"
#include "Misc/CoreMiscDefines.h" 
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderResource.h"
#include "RHIUtilities.h"
#include "Runtime/Launch/Resources/Version.h"
#include "ShaderCompiler.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"

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
	TArray<int32> SampleIndexes;
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

	TArray<float> FloatData;
	TArray<float> SpawnTimes;
	TArray<float> LifeValues;
	TArray<int32> PointTypes;
	TArray<int32> SpecialAttributeIndexes;
	TArray<int32> PointValueIndexes;
	TArray<FString> Attributes;

	int32 NumSamples;
	int32 NumAttributes;
	int32 NumPoints;
	int32 MaxNumIndexesPerPoint;
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

	int32 MaxNumberOfIndexesPerPoint;
	int32 NumSamples;
	int32 NumAttributes;
	int32 NumPoints;

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
	
	//-----------------------------------------------------------------------------------------
	//  MEMBER FUNCTIONS
	//-----------------------------------------------------------------------------------------

#if WITH_EDITOR
	bool UpdateFromFile( const FString& TheFileName );
#endif

	void SetFileName( const FString& TheFilename );

	// Returns the number of points found in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	int32 GetNumberOfPoints() const;

	// Returns the number of samples found in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	int32 GetNumberOfSamples() const;

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
	bool GetFloatValue( const int32& sampleIndex, const int32& attrIndex, float& value ) const;
	// Returns the float value at a given point in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetFloatValueForString( const int32& sampleIndex, const FString& Attribute, float& value ) const;
	/*
	// Returns the string value at a given point in the point cache
	bool GetCSVStringValue( const int32& sampleIndex, const int32& attrIndex, FString& value );
	// Returns the string value at a given point in the point cache
	bool GetCSVStringValue( const int32& sampleIndex, const FString& Attribute, FString& value );
	*/
	// Returns a Vector3 for a given point in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetVectorValue( const int32& sampleIndex, const int32& attrIndex, FVector& value, const bool& DoSwap = true, const bool& DoScale = true ) const;
	// Returns a Vector3 for a given point in the point cache by column name
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetVectorValueForString(const int32& sampleIndex, const FString& Attribute, FVector& value, const bool& DoSwap = true, const bool& DoScale = true) const;
	// Returns a Vector4 for a given point in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetVector4Value( const int32& sampleIndex, const int32& attrIndex, FVector4& value ) const;
	// Returns a Vector4 for a given point in the point cache by column name
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetVector4ValueForString(const int32& sampleIndex, const FString& Attribute, FVector4& value ) const;
	// Returns a Quat for a given point in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetQuatValue( const int32& sampleIndex, const int32& attrIndex, FQuat& value, const bool& DoHoudiniToUnrealConversion = true ) const;
	// Returns a Quat for a given point in the point cache by column name
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetQuatValueForString(const int32& sampleIndex, const FString& Attribute, FQuat& value, const bool& DoHoudiniToUnrealConversion = true ) const;

	// Returns a time value for a given point in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetTimeValue( const int32& sampleIndex, float& value ) const;
	// Returns a Position Vector3 for a given point in the point cache (converted to unreal's coordinate system)
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPositionValue( const int32& sampleIndex, FVector& value ) const;
	// Returns a Normal Vector3 for a given point in the point cache (converted to unreal's coordinate system)
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetNormalValue( const int32& sampleIndex, FVector& value ) const;
	// Returns a Color for a given point in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetColorValue( const int32& sampleIndex, FLinearColor& value ) const;
	// Returns a Velocity Vector3 for a given point in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetVelocityValue(const int32& sampleIndex, FVector& value ) const;
	// Returns an Impulse float value for a given point in the point cache
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetImpulseValue(const int32& sampleIndex, float& value) const;

	// Get the last sample index for a given time value (the sample with a time smaller or equal to desiredTime)
	// If the point cache doesn't have time informations, returns false and set LastsampleIndex to the last sample in the file
	// If desiredTime is smaller than the time value in the first sample, LastsampleIndex will be set to -1
	// If desiredTime is higher than the last time value in the last sample of the point cache, LastIndex will be set to the last sample's index
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetLastSampleIndexAtTime( const float& desiredTime, int32& lastSampleIndex ) const;

	// Get the last pointID of the points to be spawned at time t
	// Invalid Index are used to indicate edge cases:
	// -1 will be returned if there is no points to spawn ( t is smaller than the first point time )
	// NumberOfSamples will be returned if all points in the CSV have been spawned ( t is higher than the last point time )
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetLastPointIDToSpawnAtTime( const float& time, int32& lastID ) const;
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointIDsToSpawnAtTime(
		const float& desiredTime,
		int32& MinID, int32& MaxID, int32& Count,
		int32& LastSpawnedPointID, float& LastSpawnTime, float& LastSpawnTimeRequest) const;

	bool GetPointIDsToSpawnAtTime_DEPR(
		const float& desiredTime,
		int32& MinID, int32& MaxID, int32& Count,
		int32& LastSpawnedPointID, float& LastSpawnTime ) const;

	// Returns the previous and next sample indexes for reading the values of a specified point at a given time
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetSampleIndexesForPointAtTime(const int32& PointID, const float& desiredTime, int32& PrevSampleIndex, int32& NextSampleIndex, float& PrevWeight) const;
	// Returns the value for a point at a given time value (linearly interpolated)
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointValueAtTime(const int32& PointID, const int32& AttributeIndex, const float& desiredTime, float& Value) const;
	// Returns the value for a point at a given time value (linearly interpolated), via the attribute name
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointValueAtTimeForString(const int32& PointID, const FString& Attribute, const float& desiredTime, float& Value) const;
	
	// Returns the Vector Value for a given point at a given time value (linearly interpolated)
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointVectorValueAtTime(int32 PointID, int32 AttributeIndex, float desiredTime, FVector& Vector, bool DoSwap, bool DoScale) const;
	
	// Returns the Vector Value for a given point at a given time value (linearly interpolated), via the attribute name
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointVectorValueAtTimeForString(int32 PointID, const FString& Attribute, float desiredTime, FVector& Vector, bool DoSwap, bool DoScale) const;

	// Returns the Vector4 Value for a given point at a given time value (linearly interpolated)
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointVector4ValueAtTime(int32 PointID, int32 AttributeIndex, float desiredTime, FVector4& Vector) const;
	
	// Returns the Vector4 Value for a given point at a given time value (linearly interpolated), via the attribute name
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointVector4ValueAtTimeForString(int32 PointID, const FString& Attribute, float desiredTime, FVector4& Vector) const;

	// Returns the Quat Value for a given point at a given time value (linearly interpolated)
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointQuatValueAtTime(int32 PointID, int32 AttributeIndex, float desiredTime, FQuat& Quat, bool DoHoudiniToUnrealConversion = true ) const;
	
	// Returns the Quat Value for a given point at a given time value (linearly interpolated), via the attribute name
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointQuatValueAtTimeForString(int32 PointID, const FString& Attribute, float desiredTime, FQuat& Quat, bool DoHoudiniToUnrealConversion = true ) const;

	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointFloatValueAtTime(int32 PointID, int32 AttributeIndex, float desiredTime, float& Value) const;

	// Return the integer value of the point at the keyframe before the desired time. No value interpolation will take place.
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointInt32ValueAtTime(int32 PointID, int32 AttributeIndex, float desiredTime, int32& Value) const;
	
	// Returns the Position Value for a given point at a given time value (linearly interpolated)
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointPositionAtTime(const int32& PointID, const float& desiredTime, FVector& Vector) const;
	// Return a given point's life value at spawn
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointLife(const int32& PointID, float& Value) const;
	// Return a point's life for a given time value
	// Note this function currently behaves exactly the same as GetPointLife
	// since the Lifetime value is currently treated as a constant. This could
	// change in the future.
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointLifeAtTime(const int32& PointID, const float& DesiredTime, float& Value) const;
	// Return a point's type at spawn
	UFUNCTION(BlueprintCallable, Category = "Houdini Attributes Data")
	bool GetPointType(const int32& PointID, int32& Value) const;


	// Returns the maximum number of indexes per point, used for flattening the buffer for HLSL conversion
	int32 GetMaxNumberOfPointValueIndexes() const;

	//-----------------------------------------------------------------------------------------
	//  MEMBER VARIABLES
	//-----------------------------------------------------------------------------------------

	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties" )
	FString FileName;

	// The number of values stored in the point cache
	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties" )
	int32 NumberOfSamples;

	// The number of attributes stored in the point cache
	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties" )
	int32 NumberOfAttributes;

	// The number of unique points found in the point cache
	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties")
	int32 NumberOfPoints;

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
	class UAssetImportData* AssetImportData;

	// Raw data of the source file so that we can export it again.
	UPROPERTY()
	TArray<uint8> RawDataCompressed;

	// Compression scheme used to compress raw 
	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties" )
	FName RawDataFormatID;

	// Size of data when uncompressed
	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties" )
	uint32 RawDataUncompressedSize;

	// Compression scheme used to compress raw 
	UPROPERTY( VisibleAnywhere, Category = "Houdini Point Cache Properties" )
	FName RawDataCompressionMethod;
#endif

#if WITH_EDITOR
	bool HasRawData() const { return RawDataCompressed.Num() > 0; };

	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent & PropertyChangedEvent) override;
#endif

	virtual void GetAssetRegistryTags(TArray< FAssetRegistryTag > & OutTags) const override;
	
	void BeginDestroy() override;

	// Data Accessors, const and non-const versions
	TArray<float>& GetFloatSampleData() { return FloatSampleData; }

	UFUNCTION(BlueprintCallable, Category = "Houdini Point Cache Data")
	const TArray<float>& GetFloatSampleData() const { return FloatSampleData; }

	TArray<float>& GetSpawnTimes() { return SpawnTimes; }

	UFUNCTION(BlueprintCallable, Category = "Houdini Point Cache Data")
	const TArray<float>& GetSpawnTimes() const { return SpawnTimes; }

	TArray<float>& GetLifeValues() { return LifeValues; }

	UFUNCTION(BlueprintCallable, Category = "Houdini Point Cache Data")
	const TArray<float>& GetLifeValues() const { return LifeValues; }

	TArray<int32>& GetPointTypes() { return PointTypes; }

	UFUNCTION(BlueprintCallable, Category = "Houdini Point Cache Data")
	const TArray<int32>& GetPointTypes() const { return PointTypes; }

	TArray<int32>& GetSpecialAttributeIndexes() { return SpecialAttributeIndexes; }

	UFUNCTION(BlueprintCallable, Category = "Houdini Point Cache Data")
	const TArray<int32>& GetSpecialAttributeIndexes() const { return SpecialAttributeIndexes; }

	TArray<FPointIndexes>& GetPointValueIndexes() { return PointValueIndexes; }

	UFUNCTION()
	const TArray<FPointIndexes>& GetPointValueIndexes() const { return PointValueIndexes; }

	UFUNCTION(BlueprintCallable, Category = "Houdini Point Cache Settings")
	bool GetUseCustomCSVTitleRow() const { return UseCustomCSVTitleRow; }

	UFUNCTION(BlueprintCallable, Category = "Houdini Point Cache Settings")
	void SetUseCustomCSVTitleRow(bool bInUseCustomCSVTitleRow) { UseCustomCSVTitleRow = bInUseCustomCSVTitleRow; }

	/** The GPU resource for this point cache. */
	TUniquePtr<class FHoudiniPointCacheResource> Resource;

	void RequestPushToGPU();

	private:

	/*
	// Array containing the Raw String data
	UPROPERTY()
	TArray<FString> StringCSVData;
	*/

	// Array containing all the sample data converted to floats
	UPROPERTY()
	TArray<float> FloatSampleData;
	
	// Array containing the spawn times for each point in the point cache
	UPROPERTY()
	TArray<float> SpawnTimes;

	// Array containing all the life values for each point in the point cache
	UPROPERTY()
	TArray<float> LifeValues;

	// Array containing all the type values for each point in the point cache
	UPROPERTY()
	TArray<int32> PointTypes;

	// Array containing the column indexes of the special attributes
	UPROPERTY()
	TArray<int32> SpecialAttributeIndexes;

	/*
	// Row indexes for new time values
	UPROPERTY()
	TMap<float, int32> TimeValuesIndexes;
	*/

	// Sample indexes for each point
	UPROPERTY()
	TArray< FPointIndexes > PointValueIndexes;

	/** For CSV source files, whether to use a custom title row. */
	UPROPERTY()
	bool UseCustomCSVTitleRow;

	// The type of source file, such as CSV or JSON.
	UPROPERTY()
	EHoudiniPointCacheFileType FileType;
};