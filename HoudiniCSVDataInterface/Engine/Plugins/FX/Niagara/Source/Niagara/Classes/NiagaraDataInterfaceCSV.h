// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "VectorVM.h"
#include "StaticMeshResources.h"
#include "Curves/RichCurve.h"
#include "Engine/DataTable.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceCSV.generated.h"

class INiagaraCompiler;
class UCurveVector;
class UCurveLinearColor;
class UCurveFloat;
class FNiagaraSystemInstance;


/** Data Interface allowing sampling of vector curves. */
UCLASS(EditInlineNew, Category = "CSV", meta = (DisplayName = "CSV File Importer"))
class NIAGARA_API UNiagaraDataInterfaceCSV : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "CSV File" )
	FString CSVFileName;

	void UpdateDataFromCSVFile();

	//UObject Interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//UObject Interface End

	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	virtual FVMExternalFunction GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData)override;

	// EXPOSED FUNCTIONS

	// Returns the positions for a given point in the CSV file
	template<typename NParamType>
	void SampleCSV(FVectorVMContext& Context);

	// Returns the number of points found in the CSV file
	void GetNumberOfPointsInCSV(FVectorVMContext& Context);

	virtual bool CopyTo(UNiagaraDataInterface* Destination) const override;

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	//~ UNiagaraDataInterfaceCSVBase interface

	virtual bool GetFunctionHLSL(FString FunctionName, TArray<DIGPUBufferParamDescriptor> &Descriptors, FString &HLSLInterfaceID, FString &OutHLSL) override;
	virtual void GetBufferDefinitionHLSL(FString DataInterfaceID, TArray<DIGPUBufferParamDescriptor> &BufferDescriptors, FString &OutHLSL) override;
	virtual TArray<FNiagaraDataInterfaceBufferData> &GetBufferDataArray() override;
	virtual void SetupBuffers(TArray<DIGPUBufferParamDescriptor> &BufferDescriptors) override;

	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const override { return true; }

protected:

	UPROPERTY()
	bool GPUBufferDirty; 
    
	// Array containing all the row CSV float data
	UPROPERTY()
	TArray<float> CSVData;

	// The tokenized title row, describing the content of each title
	UPROPERTY()
	TArray<FString> TitleRowArray;

	// Array containing the Position buffer
	UPROPERTY()
	TArray<float> PositionData;
    
	// The number of values stored in the CSV file (excluding the title row)
	int32 NumberOfRows;

	// The number of value TYPES stored in the CSV file
	int32 NumberOfColumns;
};
