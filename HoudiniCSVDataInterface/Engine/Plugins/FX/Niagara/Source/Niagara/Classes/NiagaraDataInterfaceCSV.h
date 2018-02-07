/*
* Copyright (c) <2017> Side Effects Software Inc.
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


/** Data Interface allowing sampling of Houdini CSV files. */
UCLASS(EditInlineNew, Category = "CSV", meta = (DisplayName = "CSV File Importer"))
class NIAGARA_API UNiagaraDataInterfaceCSV : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "CSV File", meta = (DisplayName = "CSV File Path") )
	FFilePath CSVFileName;

	void UpdateDataFromCSVFile();

	//----------------------------------------------------------------------------
	//UObject Interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//----------------------------------------------------------------------------

	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	
	/** Returns the delegate for the passed function signature. */
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	//----------------------------------------------------------------------------
	// EXPOSED FUNCTIONS

	// Returns the float value at a given point in the CSV file
	template<typename RowParamType, typename ColParamType>
	void GetCSVFloatValue(FVectorVMContext& Context);

	// Returns the positions for a given point in the CSV file
	template<typename NParamType>
	void GetCSVPosition(FVectorVMContext& Context);

	// Returns the normals for a given point in the CSV file
	template<typename NParamType>
	void GetCSVNormal(FVectorVMContext& Context);

	// Returns the time for a given point in the CSV file
	template<typename NParamType>
	void GetCSVTime(FVectorVMContext& Context);

	/*
	// Returns the position and time for a given point in the CSV file
	template<typename NParamType>
	void GetCSVPositionAndTime(FVectorVMContext& Context);
	*/

	// Returns the number of points found in the CSV file
	void GetNumberOfPointsInCSV(FVectorVMContext& Context);
	
	//----------------------------------------------------------------------------
	// GPU / HLSL Functions
	virtual bool GetFunctionHLSL(const FName& DefinitionFunctionName, FString InstanceFunctionName, TArray<FDIGPUBufferParamDescriptor> &Descriptors, FString &HLSLInterfaceID, FString &OutHLSL) override;
	virtual void GetBufferDefinitionHLSL(FString DataInterfaceID, TArray<FDIGPUBufferParamDescriptor> &BufferDescriptors, FString &OutHLSL) override;
	virtual TArray<FNiagaraDataInterfaceBufferData> &GetBufferDataArray() override;
	virtual void SetupBuffers(FDIBufferDescriptorStore &BufferDescriptors) override;

	// Disabling GPU sim for now.
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const override { return Target == ENiagaraSimTarget::CPUSim; }

protected:

	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

	//----------------------------------------------------------------------------
	// MEMBER VARIABLES

	// Array containing all the row CSV data converted to float
	UPROPERTY()
	TArray<float> CSVData;

	// The tokenized title row, describing the content of each title
	UPROPERTY()
	TArray<FString> TitleRowArray;

	// Array containing the Position buffer
	UPROPERTY()
	TArray<float> PositionData;

	// Array containing the Normal buffer
	UPROPERTY()
	TArray<float> NormalData;

	// Array comtinting the time buffer
	UPROPERTY()
	TArray<float> TimeData;

	// The number of values stored in the CSV file (excluding the title row)
	UPROPERTY()
	int32 NumberOfRows;

	// The number of value TYPES stored in the CSV file
	UPROPERTY()
	int32 NumberOfColumns;

	// Indicates the GPU buffers need to be updated
	UPROPERTY()
	bool GPUBufferDirty;
};
