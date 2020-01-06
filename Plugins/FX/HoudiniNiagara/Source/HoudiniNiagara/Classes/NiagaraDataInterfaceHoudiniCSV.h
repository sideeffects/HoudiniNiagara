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
#include "UObject/ObjectMacros.h"
#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "VectorVM.h"
#include "HoudiniCSV.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceHoudiniCSV.generated.h"

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


/** Data Interface allowing sampling of Houdini CSV files. */
UCLASS(EditInlineNew, Category = "Houdini Niagara", meta = (DisplayName = "Houdini Array Info"))
class HOUDININIAGARA_API UNiagaraDataInterfaceHoudiniCSV : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:

	// Houdini CSV Asset to sample
	UPROPERTY( EditAnywhere, Category = "Houdini Niagara", meta = (DisplayName = "Houdini CSV Asset" ) )
	UHoudiniCSV* HoudiniCSVAsset;

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

	// Returns the float value at a given row and column in the CSV file
	void GetFloatValue(FVectorVMContext& Context);

	/*
	template<typename RowParamType, typename ColTitleParamType>
	void GetFloatValueByString(FVectorVMContext& Context);
	*/

	// Returns a Vector3 value for a given row in the CSV file
	void GetVectorValue(FVectorVMContext& Context);

	// Returns a Vector3 value for a given row in the CSV file
	void GetVectorValueEx(FVectorVMContext& Context);

	// Returns the positions for a given row in the CSV file
	void GetPosition(FVectorVMContext& Context);

	// Returns the normals for a given row in the CSV file
	void GetNormal(FVectorVMContext& Context);

	// Returns the time for a given row in the CSV file
	void GetTime(FVectorVMContext& Context);

	// Returns the velocity for a given row in the CSV file
	void GetVelocity(FVectorVMContext& Context);

	// Returns the color for a given row in the CSV file
	void GetColor(FVectorVMContext& Context);

	// Returns the impulse value for a given row in the CSV file
	void GetImpulse(FVectorVMContext& Context);

	// Returns the position and time for a given row in the CSV file
	void GetPositionAndTime(FVectorVMContext& Context);

	// Returns the number of rows found in the CSV file
	void GetNumberOfRows(FVectorVMContext& Context);

	// Returns the number of columns found in the CSV file
	void GetNumberOfColumns(FVectorVMContext& Context);

	// Returns the number of points found in the CSV file
	void GetNumberOfPoints(FVectorVMContext& Context);

	// Returns the last index of the points that should be spawned at time t
	void GetLastRowIndexAtTime(FVectorVMContext& Context);

	// Returns the indexes (min, max) and number of points that should be spawned at time t
	void GetPointIDsToSpawnAtTime(FVectorVMContext& Context);

	// Returns the position for a given point at a given time
	void GetPointPositionAtTime(FVectorVMContext& Context);

	// Returns a float value for a given point at a given time
	void GetPointValueAtTime(FVectorVMContext& Context);

	// Returns a Vector value for a given point at a given time
	void GetPointVectorValueAtTime(FVectorVMContext& Context);

	// Returns a Vector value for a given point at a given time
	void GetPointVectorValueAtTimeEx(FVectorVMContext& Context);

	// Returns the line indexes (previous, next) for reading values for a given point at a given time
	void GetRowIndexesForPointAtTime(FVectorVMContext& Context);

	// Return the life value for a given point
	void GetPointLife(FVectorVMContext& Context);

	// Return the life of a given point at a given time
	//template<typename PointIDParamType, typename TimeParamType>
	//void GetPointLifeAtTime(FVectorVMContext& Context);

	// Return the type value for a given point
	void GetPointType( FVectorVMContext& Context );
	
	//----------------------------------------------------------------------------
	// GPU / HLSL Functions
	virtual bool GetFunctionHLSL(const FName& DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual void GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual FNiagaraDataInterfaceParametersCS* ConstructComputeParameters() const override;

	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const override { return true; }

	// Members for GPU compatibility

	// Buffer and member variables base name
	static const FString NumberOfRowsBaseName;
	static const FString NumberOfColumnsBaseName;
	static const FString NumberOfPointsBaseName;
	static const FString FloatValuesBufferBaseName;
	static const FString SpecialAttributesColumnIndexesBufferBaseName;
	static const FString SpawnTimesBufferBaseName;
	static const FString LifeValuesBufferBaseName;
	static const FString PointTypesBufferBaseName;
	static const FString MaxNumberOfIndexesPerPointBaseName;
	static const FString PointValueIndexesBufferBaseName;
	static const FString LastSpawnedPointIdBaseName;
	static const FString LastSpawnTimeBaseName;

	// Member variables accessors
	FORCEINLINE int32 GetNumberOfRows()const { return HoudiniCSVAsset ? HoudiniCSVAsset->GetNumberOfRows() : 0; }
	FORCEINLINE int32 GetNumberOfColumns()const { return HoudiniCSVAsset ? HoudiniCSVAsset->GetNumberOfColumns() : 0; }
	FORCEINLINE int32 GetNumberOfPoints()const { return HoudiniCSVAsset ? HoudiniCSVAsset->GetNumberOfPoints() : 0; }
	FORCEINLINE int32 GetMaxNumberOfIndexesPerPoints()const { return HoudiniCSVAsset ? HoudiniCSVAsset->GetMaxNumberOfPointValueIndexes() + 1 : 0; }

	// GPU Buffers accessors
	/*FRWBuffer& GetFloatValuesGPUBuffer();
	FRWBuffer& GetSpecialAttributesColumnIndexesGPUBuffer();
	FRWBuffer& GetSpawnTimesGPUBuffer();
	FRWBuffer& GetLifeValuesGPUBuffer();
	FRWBuffer& GetPointTypesGPUBuffer();
	FRWBuffer& GetPointValueIndexesGPUBuffer();*/

protected:

	void PushToRenderThread();

	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

	// Last Spawned PointID
	UPROPERTY()
	int32 LastSpawnedPointID;
	// Last Spawn time
	UPROPERTY()
	float LastSpawnTime;
};

struct FNiagaraDataInterfaceProxyHoudiniCSV : public FNiagaraDataInterfaceProxy
{
	// GPU Buffers
	FRWBuffer FloatValuesGPUBuffer;
	FRWBuffer SpecialAttributesColumnIndexesGPUBuffer;
	FRWBuffer SpawnTimesGPUBuffer;
	FRWBuffer LifeValuesGPUBuffer;
	FRWBuffer PointTypesGPUBuffer;
	FRWBuffer PointValueIndexesGPUBuffer;

	int32 MaxNumberOfIndexesPerPoint;
	int32 NumRows;
	int32 NumColumns;
	int32 NumPoints;

	void AcceptStaticDataUpdate(struct FNiagaraDIHoudiniCSV_StaticDataPassToRT& Update);

	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override
	{
		return 0;
	}
};