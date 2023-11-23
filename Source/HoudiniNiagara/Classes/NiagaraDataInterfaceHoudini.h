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

#include "HoudiniPointCache.h"

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "NiagaraDataInterface.h"
#include "NiagaraShared.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/ObjectMacros.h"
#include "VectorVM.h"

#include "NiagaraDataInterfaceHoudini.generated.h"

#if ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION < 2 
	#if WITH_EDITORONLY_DATA
		#define HOUDINI_COMPILE_NIAGARA
	#endif
#else
	#define HOUDINI_COMPILE_NIAGARA
#endif

USTRUCT()
struct FHoudiniEvent
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Houdini Event")
	FVector Position;

	UPROPERTY(EditAnywhere, Category = "Houdini Event")
	FVector Normal;

	UPROPERTY(EditAnywhere, Category = "Houdini Event")
	float Impulse;

	UPROPERTY(EditAnywhere, Category = "Houdini Event")
	FVector Velocity;

	UPROPERTY(EditAnywhere, Category = "Houdini Event")
	int32 PointID;

	UPROPERTY(EditAnywhere, Category = "Houdini Event")
	float Time;	

	UPROPERTY(EditAnywhere, Category = "Houdini Event")
	float Life;

	UPROPERTY(EditAnywhere, Category = "Houdini Event")
	FLinearColor Color;

	UPROPERTY(EditAnywhere, Category = "Houdini Event")
	int32 Type;	

	FHoudiniEvent()
		: Position(FVector::ZeroVector)
		, Normal(FVector::ZeroVector)
		, Impulse(0.0f)
		, Velocity( FVector::ZeroVector)
		, PointID(0)
		, Time(0.0f)
		, Life(0.0f)
		, Color(FLinearColor::White)
		, Type(0)		 
	{
	}

	inline bool operator==(const FHoudiniEvent& Other) const
	{
		if ( (Other.Position != Position) || (Other.Normal != Normal)
			|| (Other.Velocity != Velocity) || (Other.Impulse != Impulse)
			|| (Other.PointID != PointID) || (Other.Time != Time)
			|| (Other.Life != Life) || (Other.Color != Color)
			|| (Other.Type != Type) )
			return false;

		return true;
	}

	inline bool operator!=(const FHoudiniEvent& Other) const
	{
		return !(*this == Other);
	}
};


/** Data Interface allowing sampling of UHoudiniPointCache assets (CSV, .json (binary) files) files. */
UCLASS(EditInlineNew, Category = "Houdini Niagara", meta = (DisplayName = "Houdini Point Cache Info"))
class HOUDININIAGARA_API UNiagaraDataInterfaceHoudini : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

#if ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION < 1
public:
	DECLARE_NIAGARA_DI_PARAMETER();
	
#else
	BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters, )
		SHADER_PARAMETER(int32, NumberOfSamples)
		SHADER_PARAMETER(int32, NumberOfAttributes)
		SHADER_PARAMETER(int32, NumberOfPoints)
		SHADER_PARAMETER(int32, MaxNumberOfIndexesPerPoint)
		SHADER_PARAMETER(int32, LastSpawnedPointId)
		SHADER_PARAMETER(float, LastSpawnTime)
		SHADER_PARAMETER(float, LastSpawnTimeRequest)
		SHADER_PARAMETER_SRV(Buffer<float>, FloatValuesBuffer)
		SHADER_PARAMETER_SRV(Buffer<int>, SpecialAttributeIndexesBuffer)
		SHADER_PARAMETER_SRV(Buffer<float>, SpawnTimesBuffer)
		SHADER_PARAMETER_SRV(Buffer<float>, LifeValuesBuffer)
		SHADER_PARAMETER_SRV(Buffer<float>, PointTypesBuffer)
		SHADER_PARAMETER_SRV(Buffer<int>, PointValueIndexesBuffer)
		SHADER_PARAMETER_SRV(Buffer<uint>, FunctionIndexToAttributeIndexBuffer)
	END_SHADER_PARAMETER_STRUCT()
public:

#endif

	UPROPERTY(EditAnywhere, Category = "Houdini Niagara", meta = (DisplayName = "Houdini Point Cache Asset"))
	TObjectPtr<UHoudiniPointCache> HoudiniPointCacheAsset;
//#if ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION < 1
//		UHoudiniPointCache* HoudiniPointCacheAsset;
//#else
//		// Houdini Point Cache Asset to sample
//		TObjectPtr<UHoudiniPointCache> HoudiniPointCacheAsset;
//#endif


	//----------------------------------------------------------------------------
	// UObject Interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//----------------------------------------------------------------------------
	// UNiagaraDataInterface Interface

	// Returns the DI's functions signatures 
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	
	// Returns the delegate for the passed function signature.
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	//----------------------------------------------------------------------------
	// EXPOSED FUNCTIONS

	// Returns the float value at a given sample index and attribute index in the point cache 
	void GetFloatValue(FVectorVMExternalFunctionContext& Context);

	// Returns a float value for a given sample index and attribute name in the point cache
	void GetFloatValueByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute);

	// Returns a Vector3 value for a given sample index in the point cache
	void GetVectorValue(FVectorVMExternalFunctionContext& Context);

	// Returns a Vector3 value for a given sample index in the point cache by attribute
	void GetVectorValueByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute);

	// Returns a Vector3 value for a given sample index in the point cache
	void GetVectorValueEx(FVectorVMExternalFunctionContext& Context);

	// Returns a Vector3 value for a given sample index in the point cache by attribute
	void GetVectorValueExByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute);

	// Returns a Vector4 value for a given sample index in the point cache
	void GetVector4Value(FVectorVMExternalFunctionContext& Context);

	// Returns a Vector4 value for a given sample index in the point cache by attribute
	void GetVector4ValueByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute);

	// Returns a Quat value for a given sample index in the point cache
	void GetQuatValue(FVectorVMExternalFunctionContext& Context);

	// Returns a Quat value for a given sample index in the point cache by attribute
	void GetQuatValueByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute);

	// Returns the positions for a given sample index in the point cache
	void GetPosition(FVectorVMExternalFunctionContext& Context);

	// Returns the normals for a given sample index in the point cache
	void GetNormal(FVectorVMExternalFunctionContext& Context);

	// Returns the time for a given sample index in the point cache
	void GetTime(FVectorVMExternalFunctionContext& Context);

	// Returns the velocity for a given sample index in the point cache
	void GetVelocity(FVectorVMExternalFunctionContext& Context);

	// Returns the color for a given sample index in the point cache
	void GetColor(FVectorVMExternalFunctionContext& Context);

	// Returns the impulse value for a given sample index in the point cache
	void GetImpulse(FVectorVMExternalFunctionContext& Context);

	// Returns the position and time for a given sample index in the point cache
	void GetPositionAndTime(FVectorVMExternalFunctionContext& Context);

	// Returns the number of samples found in the point cache
	void GetNumberOfSamples(FVectorVMExternalFunctionContext& Context);

	// Returns the number of attributes in the point cache
	void GetNumberOfAttributes(FVectorVMExternalFunctionContext& Context);

	// Returns the number of unique points (by id) in the point cache
	void GetNumberOfPoints(FVectorVMExternalFunctionContext& Context);

	// Returns the last sample index of the points that should be spawned at time t
	void GetLastSampleIndexAtTime(FVectorVMExternalFunctionContext& Context);

	// Returns the indexes (min, max) and number of points that should be spawned at time t
	void GetPointIDsToSpawnAtTime(FVectorVMExternalFunctionContext& Context);

	// Returns the position for a given point at a given time
	void GetPointPositionAtTime(FVectorVMExternalFunctionContext& Context);

	// Returns a float value for a given point at a given time 
	void GetPointValueAtTime(FVectorVMExternalFunctionContext& Context);

	// Returns a float value by attribute name for a given point at a given time 
	void GetPointValueAtTimeByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute);

	// Returns a Vector value for a given point at a given time
	void GetPointVectorValueAtTime(FVectorVMExternalFunctionContext& Context);

	// Returns a Vector value by attribute name for a given point at a given time
	void GetPointVectorValueAtTimeByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute);

	// Returns a Vector value for a given point at a given time
	void GetPointVectorValueAtTimeEx(FVectorVMExternalFunctionContext& Context);

	// Returns a Vector value by attribute name for a given point at a given time
	void GetPointVectorValueAtTimeExByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute);

	// Returns a Vector4 value for a given point at a given time
	void GetPointVector4ValueAtTime(FVectorVMExternalFunctionContext& Context);

	// Returns a Vector4 value by attribute name for a given point at a given time
	void GetPointVector4ValueAtTimeByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute);

	// Returns a Quat value for a given point at a given time
	void GetPointQuatValueAtTime(FVectorVMExternalFunctionContext& Context);

	// Returns a Quat value by attribute name for a given point at a given time
	void GetPointQuatValueAtTimeByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute);

	// Returns the sample indexes (previous, next) for reading values for a given point at a given time
	void GetSampleIndexesForPointAtTime(FVectorVMExternalFunctionContext& Context);

	// Return the life value for a given point
	void GetPointLife(FVectorVMExternalFunctionContext& Context);

	// Return the life of a given point at a given time
	//template<typename PointIDParamType, typename TimeParamType>
	void GetPointLifeAtTime(FVectorVMExternalFunctionContext& Context);

	// Return the type value for a given point
	void GetPointType(FVectorVMExternalFunctionContext& Context);

	//----------------------------------------------------------------------------
	// Standard attribute accessor as a stopgap. These can be removed when
	// string-based attribute lookups work on both the CPU and GPU.
	
	// Sample a vector attribute value for a given point at a given time
	void GetPointGenericVectorAttributeAtTime(EHoudiniAttributes Attribute, FVectorVMExternalFunctionContext& Context, bool DoSwap, bool DoScale);
	
	// Sample a float attribute value for a given point at a given time
	void GetPointGenericFloatAttributeAtTime(EHoudiniAttributes Attribute, FVectorVMExternalFunctionContext& Context);

	// Sample a float attribute value for a given point at a given time
	void GetPointGenericInt32AttributeAtTime(EHoudiniAttributes Attribute, FVectorVMExternalFunctionContext& Context);

	void GetPointNormalAtTime(FVectorVMExternalFunctionContext& Context);
	
	void GetPointColorAtTime(FVectorVMExternalFunctionContext& Context);
	
	void GetPointAlphaAtTime(FVectorVMExternalFunctionContext& Context);
	
	void GetPointVelocityAtTime(FVectorVMExternalFunctionContext& Context);
	
	void GetPointImpulseAtTime(FVectorVMExternalFunctionContext& Context);

	void GetPointTypeAtTime(FVectorVMExternalFunctionContext& Context);

	//----------------------------------------------------------------------------
	// GPU / HLSL Functions

#if ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION < 1
#if WITH_EDITORONLY_DATA
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual void GetCommonHLSL(FString& OutHLSL) override;
#endif
#else

#if WITH_EDITORONLY_DATA
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual void GetCommonHLSL(FString& OutHLSL) override;
#endif


	//virtual bool UseLegacyShaderBindings() const override { return false; }
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;
	virtual FNiagaraDataInterfaceParametersCS* CreateShaderStorage(const FNiagaraDataInterfaceGPUParamInfo& ParameterInfo, const FShaderParameterMap& ParameterMap) const override;
	virtual const FTypeLayoutDesc* GetShaderStorageType() const override;

#endif

	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const override { return true; }

	// Members for GPU compatibility

	// Buffer and member variables base name
	static const FString NumberOfSamplesBaseName;
	static const FString NumberOfAttributesBaseName;
	static const FString NumberOfPointsBaseName;
	static const FString FloatValuesBufferBaseName;
	static const FString SpecialAttributeIndexesBufferBaseName;
	static const FString SpawnTimesBufferBaseName;
	static const FString LifeValuesBufferBaseName;
	static const FString PointTypesBufferBaseName;
	static const FString MaxNumberOfIndexesPerPointBaseName;
	static const FString PointValueIndexesBufferBaseName;
	static const FString LastSpawnedPointIdBaseName;
	static const FString LastSpawnTimeBaseName;
	static const FString LastSpawnTimeRequestBaseName;
	static const FString FunctionIndexToAttributeIndexBufferBaseName;

	// Member variables accessors
	FORCEINLINE int32 GetNumberOfSamples()const { return HoudiniPointCacheAsset ? HoudiniPointCacheAsset->GetNumberOfSamples() : 0; }
	FORCEINLINE int32 GetNumberOfAttributes()const { return HoudiniPointCacheAsset ? HoudiniPointCacheAsset->GetNumberOfAttributes() : 0; }
	FORCEINLINE int32 GetNumberOfPoints()const { return HoudiniPointCacheAsset ? HoudiniPointCacheAsset->GetNumberOfPoints() : 0; }
	FORCEINLINE int32 GetMaxNumberOfIndexesPerPoints()const { return HoudiniPointCacheAsset ? HoudiniPointCacheAsset->GetMaxNumberOfPointValueIndexes() + 1 : 0; }

	// GPU Buffers accessors
	/*FRWBuffer& GetFloatValuesGPUBuffer();
	FRWBuffer& GetSpecialAttributesColumnIndexesGPUBuffer();
	FRWBuffer& GetSpawnTimesGPUBuffer();
	FRWBuffer& GetLifeValuesGPUBuffer();
	FRWBuffer& GetPointTypesGPUBuffer();
	FRWBuffer& GetPointValueIndexesGPUBuffer();*/

protected:
	virtual void PushToRenderThreadImpl() override;

	// Get the index of the function at InFunctionIndex in InGenerationFunctions if we only count functions with
	// the Attribute function specifier.
	// Return false if the function itself does not have the Attribute specifier, or if InGeneratedFunctions is empty.
	bool GetAttributeFunctionIndex(const TArray<FNiagaraDataInterfaceGeneratedFunction>& InGeneratedFunctions, int InFunctionIndex, int &OutColTitleFunctionIndex) const;

	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

	// The following state variables are now stored in Niagara on the emitter itself
	// as opposed to being stored internally.
	// // Last Spawned PointID
	// UPROPERTY()
	// int32 LastSpawnedPointID;
	// // Last Spawn time
	// UPROPERTY()
	// float LastSpawnTime;
	
	// // Float to track last desired time of GetPointIDsToSpawnAtTime and GetLastPointIDToSpawnAtTime().
	// // This is used to detect emitter loops
	// UPROPERTY()
	// float LastSpawnTimeRequest;
};

struct FNiagaraDataInterfaceProxyHoudini : public FNiagaraDataInterfaceProxy
{
	FHoudiniPointCacheResource* Resource;

	TArray<int32> FunctionIndexToAttributeIndex;
	bool bFunctionIndexToAttributeIndexHasBeenBuilt;
	FRWBuffer FunctionIndexToAttributeIndexGPUBuffer;

	FNiagaraDataInterfaceProxyHoudini();
	virtual ~FNiagaraDataInterfaceProxyHoudini();
	
	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override
	{
		return 0;
	}

#if ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION < 1
	void UpdateFunctionIndexToAttributeIndexBuffer(const TMemoryImageArray<FName>& FunctionIndexToAttribute, bool bForceUpdate = false);
#else
	void UpdateFunctionIndexToAttributeIndexBuffer(const TMemoryImageArray<FMemoryImageName>& FunctionIndexToAttribute, bool bForceUpdate = false);
#endif

};