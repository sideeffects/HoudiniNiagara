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

#include "NiagaraDataInterfaceHoudiniCSV.h"
#include "NiagaraTypes.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE HOUDINI_NIAGARA_LOCTEXT_NAMESPACE 


UNiagaraDataInterfaceHoudiniCSV::UNiagaraDataInterfaceHoudiniCSV(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
    CSVFile = nullptr;
    LastSpawnIndex = -1;
}

void UNiagaraDataInterfaceHoudiniCSV::PostInitProperties()
{
    Super::PostInitProperties();

    if (HasAnyFlags(RF_ClassDefaultObject))
    {
	    FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), true, false, false);
    }

    GPUBufferDirty = true;
    LastSpawnIndex = -1;
}

void UNiagaraDataInterfaceHoudiniCSV::PostLoad()
{
    Super::PostLoad();
    GPUBufferDirty = true;
    LastSpawnIndex = -1;
}

#if WITH_EDITOR

void UNiagaraDataInterfaceHoudiniCSV::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceHoudiniCSV, CSVFile))
    {
	Modify();
	if ( CSVFile )
	{
	    GPUBufferDirty = true;
	    LastSpawnIndex = -1;
	}
    }
}

#endif

bool UNiagaraDataInterfaceHoudiniCSV::CopyToInternal(UNiagaraDataInterface* Destination) const
{
    if ( !Super::CopyToInternal( Destination ) )
    {
	return false;
    }

    UNiagaraDataInterfaceHoudiniCSV* CastedInterface = CastChecked<UNiagaraDataInterfaceHoudiniCSV>( Destination );
    if ( !CastedInterface )
	return false;

    CastedInterface->CSVFile = CSVFile;

    return true;
}

bool UNiagaraDataInterfaceHoudiniCSV::Equals(const UNiagaraDataInterface* Other) const
{
    if ( !Super::Equals(Other) )
    {
	return false;
    }

    const UNiagaraDataInterfaceHoudiniCSV* OtherHNCSV = CastChecked<UNiagaraDataInterfaceHoudiniCSV>(Other);

    if ( OtherHNCSV != nullptr && OtherHNCSV->CSVFile != nullptr && CSVFile )
    {
	// Just make sure the two interfaces point to the same file
	return OtherHNCSV->CSVFile->FileName.Equals( CSVFile->FileName );
    }

    return false;
}


void UNiagaraDataInterfaceHoudiniCSV::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
    {
	// GetCSVFloatValue
	FNiagaraFunctionSignature Sig;
	Sig.Name = TEXT("GetCSVFloatValue");
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));		// CSV in
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Row")));		// Row Index In
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Col")));		// Col Index In
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));	// Float Out

	OutFunctions.Add(Sig);
    }

    {
	// GetCSVFloatValueByString
	FNiagaraFunctionSignature Sig;
	Sig.Name = TEXT("GetCSVFloatValueByString");
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));		// CSV in
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Row")));		// Row Index In
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("ColTitle")));	// Col Title In
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));	// Float Out

	OutFunctions.Add(Sig);
    }

    {
	// GetCSVPosition
	FNiagaraFunctionSignature Sig;
	Sig.Name = TEXT("GetCSVPosition");
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));		// CSV in
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("N")));		// Point Number In
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));		// Vector3 Out

	OutFunctions.Add(Sig);
    }

    {
	// GetCSVPositionAndTime
	FNiagaraFunctionSignature Sig;
	Sig.Name = TEXT("GetCSVPositionAndTime");
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));		// CSV in
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("N")));		// Point Number In
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));	// Vector3 Out
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		// float Out

	OutFunctions.Add(Sig);
    }

    {
	// GetCSVNormal
	FNiagaraFunctionSignature Sig;
	Sig.Name = TEXT("GetCSVNormal");
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));		// CSV in
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("N")));		// Point Number In
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));		// Vector3 Out

	OutFunctions.Add(Sig);
    }

    {
	// GetCSVTime
	FNiagaraFunctionSignature Sig;
	Sig.Name = TEXT("GetCSVTime");
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));		// CSV in
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("N")));		// Point Number In
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));	// Float Out

	OutFunctions.Add(Sig);
    }

    {
	// GetNumberOfPointsInCSV
	FNiagaraFunctionSignature Sig;
	Sig.Name = TEXT("GetNumberOfPointsInCSV");
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));		    // CSV in
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("NumberOfPoints")));     // Int Out

	OutFunctions.Add(Sig);
    }

    {
	// GetLastParticleIndexAtTime
	FNiagaraFunctionSignature Sig;
	Sig.Name = TEXT("GetLastParticleIndexAtTime");
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));		    // CSV in
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("LastIndex")));	    // Int Out

	OutFunctions.Add(Sig);
    }

    {
	// GetParticleIndexesToSpawnAtTime
	FNiagaraFunctionSignature Sig;
	Sig.Name = TEXT("GetParticleIndexesToSpawnAtTime");
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));		    // CSV in
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("MinIndex")));	    // Int Out
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("MaxIndex")));	    // Int Out
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Count")));		    // Int Out

	OutFunctions.Add(Sig);
    }

}

// build the shader function HLSL; function name is passed in, as it's defined per-DI; that way, configuration could change
// the HLSL in the spirit of a static switch
// TODO: need a way to identify each specific function here
// 
bool UNiagaraDataInterfaceHoudiniCSV::GetFunctionHLSL(const FName& DefinitionFunctionName, FString InstanceFunctionName, TArray<FDIGPUBufferParamDescriptor> &Descriptors, FString &HLSLInterfaceID, FString &OutHLSL)
{
    //FString BufferName = Descriptors[0].BufferParamName;
    //FString SecondBufferName = Descriptors[1].BufferParamName;

    /*
    if (InstanceFunctionName.Contains( TEXT( "GetCSVFloatValue") ) )
    {
	FString BufferName = Descriptors[0].BufferParamName;
	OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float In_Row, in float In_Col, out float Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value = ") + BufferName + TEXT("[(int)(In_Row + ( In_Col * ") + FString::FromInt(NumberOfRows) + TEXT(") ) ];");
	OutHLSL += TEXT("\n}\n");
    }
    else if (InstanceFunctionName.Contains(TEXT("GetCSVPosition")))
    {
	FString BufferName = Descriptors[1].BufferParamName;
	OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float In_N, out float3 Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value.x = ") + BufferName + TEXT("[(int)(In_N) ];");
	OutHLSL += TEXT("\t Out_Value.y = ") + BufferName + TEXT("[(int)(In_N + ") + FString::FromInt(NumberOfRows) + TEXT(") ];");
	OutHLSL += TEXT("\t Out_Value.z = ") + BufferName + TEXT("[(int)(In_N + ( 2 * ") + FString::FromInt(NumberOfRows) + TEXT(") ) ];");
	OutHLSL += TEXT("\n}\n");
    }
    else if (InstanceFunctionName.Contains(TEXT("GetCSVNormal")))
    {
	FString BufferName = Descriptors[2].BufferParamName;
	OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float In_N, out float3 Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value.x = ") + BufferName + TEXT("[(int)(In_N) ];");
	OutHLSL += TEXT("\t Out_Value.y = ") + BufferName + TEXT("[(int)(In_N + ") + FString::FromInt(NumberOfRows) + TEXT(") ];");
	OutHLSL += TEXT("\t Out_Value.z = ") + BufferName + TEXT("[(int)(In_N + ( 2 * ") + FString::FromInt(NumberOfRows) + TEXT(") ) ];");
	OutHLSL += TEXT("\n}\n");
    }
    else if (InstanceFunctionName.Contains(TEXT("GetCSVTime")))
    {
	FString BufferName = Descriptors[3].BufferParamName;
	OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float In_N, out float Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value = ") + BufferName + TEXT("[(int)(In_N) ];");
	OutHLSL += TEXT("\n}\n");
    }
    else if (InstanceFunctionName.Contains( TEXT("GetNumberOfPointsInCSV") ) )
    {
	OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("( out int Out_Value ) \n{\n");
	OutHLSL += TEXT("\t Out_Value = ") + FString::FromInt(NumberOfRows) + TEXT(";");
	OutHLSL += TEXT("\n}\n");
    }
    /*else if (InstanceFunctionName.Contains(TEXT("GetCSVPositionAndTime")))
    {
	OutHLSL += TEXT("void ") + FunctionName + TEXT("(in float In_N, out float4 Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value.x = ") + BufferName + TEXT("[(int)(In_N) ];");
	OutHLSL += TEXT("\t Out_Value.y = ") + BufferName + TEXT("[(int)(In_N + ") + FString::FromInt(NumberOfRows) + TEXT(") ];");
	OutHLSL += TEXT("\t Out_Value.z = ") + BufferName + TEXT("[(int)(In_N + ( 2 * ") + FString::FromInt(NumberOfRows) + TEXT(") ) ];");
	OutHLSL += TEXT("\t Out_Value.w = ") + SecondBufferName + TEXT("[(int)(In_N) ];");
	OutHLSL += TEXT("\n}\n");
    }*/

    return !OutHLSL.IsEmpty();
}

// build buffer definition hlsl
// 1. Choose a buffer name, add the data interface ID (important!)
// 2. add a DIGPUBufferParamDescriptor to the array argument; that'll be passed on to the FNiagaraShader for binding to a shader param, that can
// then later be found by name via FindDIBufferParam for setting; 
// 3. store buffer declaration hlsl in OutHLSL
// multiple buffers can be defined at once here
//
void UNiagaraDataInterfaceHoudiniCSV::GetBufferDefinitionHLSL(FString DataInterfaceID, TArray<FDIGPUBufferParamDescriptor> &BufferDescriptors, FString &OutHLSL)
{
    FString BufferName = "CSVData" + DataInterfaceID;
    OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");
    BufferDescriptors.Add(FDIGPUBufferParamDescriptor(BufferName, 0));		// add a descriptor for shader parameter binding

    BufferName = "PositionData" + DataInterfaceID;
    OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");
    BufferDescriptors.Add(FDIGPUBufferParamDescriptor(BufferName, 1));		// add a descriptor for shader parameter binding

    BufferName = "NormalData" + DataInterfaceID;
    OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");
    BufferDescriptors.Add(FDIGPUBufferParamDescriptor(BufferName, 2));		// add a descriptor for shader parameter binding

    BufferName = "TimeData" + DataInterfaceID;
    OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");
    BufferDescriptors.Add(FDIGPUBufferParamDescriptor(BufferName, 3));		// add a descriptor for shader parameter binding
}

// called after translate, to setup buffers matching the buffer descriptors generated during hlsl translation
// need to do this because the script used during translate is a clone, including its DIs
//
void UNiagaraDataInterfaceHoudiniCSV::SetupBuffers(FDIBufferDescriptorStore &BufferDescriptors)
{
    for (FDIGPUBufferParamDescriptor &Desc : BufferDescriptors.Descriptors)
    {
	    FNiagaraDataInterfaceBufferData BufferData(*Desc.BufferParamName);	// store off the data for later use
	    GPUBuffers.Add(BufferData);
    }
}

// return the GPU buffer array (called from NiagaraInstanceBatcher to get the buffers for setting to the shader)
// we lazily update the buffer with a new LUT here if necessary
//
TArray<FNiagaraDataInterfaceBufferData> &UNiagaraDataInterfaceHoudiniCSV::GetBufferDataArray()
{
    check( IsInRenderingThread() );
    if ( GPUBufferDirty )
    {
	check( GPUBuffers.Num() > 0 );
	/*
	// CSVData buffer
	if( GPUBuffers.Num() > 0 )
	{
	    FNiagaraDataInterfaceBufferData &GPUBuffer = GPUBuffers[0];
	    GPUBuffer.Buffer.Release();
	    GPUBuffer.Buffer.Initialize(sizeof(float), CSVData.Num(), EPixelFormat::PF_R32_FLOAT);	// always allocate for up to 64 data sets
	    uint32 BufferSize = CSVData.Num() * sizeof(float);
	    int32 *BufferData = static_cast<int32*>(RHILockVertexBuffer(GPUBuffer.Buffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
	    FPlatformMemory::Memcpy(BufferData, CSVData.GetData(), BufferSize);
	    RHIUnlockVertexBuffer(GPUBuffer.Buffer.Buffer);
	}

	// Position buffer
	if ( GPUBuffers.Num() > 1 )
	{
	    FNiagaraDataInterfaceBufferData &GPUBuffer = GPUBuffers[1];
	    GPUBuffer.Buffer.Release();
	    GPUBuffer.Buffer.Initialize(sizeof(float), PositionData.Num(), EPixelFormat::PF_R32_FLOAT);	// always allocate for up to 64 data sets
	    uint32 BufferSize = PositionData.Num() * sizeof(float);
	    int32 *BufferData = static_cast<int32*>(RHILockVertexBuffer(GPUBuffer.Buffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
	    FPlatformMemory::Memcpy(BufferData, PositionData.GetData(), BufferSize);
	    RHIUnlockVertexBuffer(GPUBuffer.Buffer.Buffer);
	}

	// Normal buffer
	if (GPUBuffers.Num() > 2)
	{
	    FNiagaraDataInterfaceBufferData &GPUBuffer = GPUBuffers[2];
	    GPUBuffer.Buffer.Release();
	    GPUBuffer.Buffer.Initialize(sizeof(float), NormalData.Num(), EPixelFormat::PF_R32_FLOAT);	// always allocate for up to 64 data sets
	    uint32 BufferSize = NormalData.Num() * sizeof(float);
	    int32 *BufferData = static_cast<int32*>(RHILockVertexBuffer(GPUBuffer.Buffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
	    FPlatformMemory::Memcpy(BufferData, NormalData.GetData(), BufferSize);
	    RHIUnlockVertexBuffer(GPUBuffer.Buffer.Buffer);
	}

	// Time buffer
	if (GPUBuffers.Num() > 3)
	{
	    FNiagaraDataInterfaceBufferData &GPUBuffer = GPUBuffers[3];
	    GPUBuffer.Buffer.Release();
	    GPUBuffer.Buffer.Initialize(sizeof(float), TimeData.Num(), EPixelFormat::PF_R32_FLOAT);	// always allocate for up to 64 data sets
	    uint32 BufferSize = TimeData.Num() * sizeof(float);
	    int32 *BufferData = static_cast<int32*>(RHILockVertexBuffer(GPUBuffer.Buffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
	    FPlatformMemory::Memcpy(BufferData, TimeData.GetData(), BufferSize);
	    RHIUnlockVertexBuffer(GPUBuffer.Buffer.Buffer);
	}
	*/
	GPUBufferDirty = false;
    }

    return GPUBuffers;
}


DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVFloatValue);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVFloatValueByString);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVVectorValue);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVPosition);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVNormal);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVPositionAndTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetLastParticleIndexAtTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetParticleIndexesToSpawnAtTime);
void UNiagaraDataInterfaceHoudiniCSV::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
    if (BindingInfo.Name == TEXT("GetCSVFloatValue") && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
    {
	TNDIParamBinder<0, float, TNDIParamBinder<1, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVFloatValue)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if (BindingInfo.Name == TEXT("GetCSVFloatValueByString") && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
    {
	TNDIParamBinder<0, float, TNDIParamBinder<1, FString, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVFloatValueByString)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if (BindingInfo.Name == TEXT("GetCSVVectorValue") && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
    {
	TNDIParamBinder<0, float, TNDIParamBinder<1, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVVectorValue)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }    
    else if (BindingInfo.Name == TEXT("GetCSVPosition") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
    {
	TNDIParamBinder<0, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVPosition)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if (BindingInfo.Name == TEXT("GetCSVNormal") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
    {
	TNDIParamBinder<0, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVNormal)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if (BindingInfo.Name == TEXT("GetCSVTime") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
    {
	TNDIParamBinder<0, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVTime)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if (BindingInfo.Name == TEXT("GetCSVPositionAndTime") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 4)
    {
	TNDIParamBinder<0, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVPositionAndTime)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if ( BindingInfo.Name == TEXT("GetNumberOfPointsInCSV") && BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 1 )
    {
	OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceHoudiniCSV::GetNumberOfPointsInCSV);
    }
    else if (BindingInfo.Name == TEXT("GetLastParticleIndexAtTime") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
    {
	TNDIParamBinder<0, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetLastParticleIndexAtTime)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if (BindingInfo.Name == TEXT("GetParticleIndexesToSpawnAtTime") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
    {
	TNDIParamBinder<0, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetParticleIndexesToSpawnAtTime)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else
    {
	UE_LOG( LogHoudiniNiagara, Error, 
	    TEXT( "Could not find data interface function:\n\tName: %s\n\tInputs: %i\n\tOutputs: %i" ),
	    *BindingInfo.Name.ToString(), BindingInfo.GetNumInputs(), BindingInfo.GetNumOutputs() );
	OutFunc = FVMExternalFunction();
    }
}

template<typename RowParamType, typename ColParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetCSVFloatValue(FVectorVMContext& Context)
{
    RowParamType RowParam(Context);
    ColParamType ColParam(Context);

    FRegisterHandler<float> OutValue(Context);

    for ( int32 i = 0; i < Context.NumInstances; ++i )
    {
	float row = RowParam.Get();
	float col = ColParam.Get();
	
	float value = 0.0f;
	if ( CSVFile )
	    CSVFile->GetCSVFloatValue( row, col, value );

	*OutValue.GetDest() = value;
	RowParam.Advance();
	ColParam.Advance();
	OutValue.Advance();
    }
}

template<typename RowParamType, typename ColParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetCSVVectorValue( FVectorVMContext& Context )
{
    RowParamType RowParam(Context);
    ColParamType ColParam(Context);

    FRegisterHandler<FVector> OutValue(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
	float row = RowParam.Get();
	float col = ColParam.Get();

	FVector value = FVector::ZeroVector;
	if ( CSVFile )
	    CSVFile->GetCSVVectorValue(row, col, value);

	*OutValue.GetDest() = value;
	RowParam.Advance();
	ColParam.Advance();
	OutValue.Advance();
    }
}

template<typename RowParamType, typename ColTitleParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetCSVFloatValueByString(FVectorVMContext& Context)
{
    RowParamType RowParam(Context);
    ColTitleParamType ColTitleParam(Context);

    FRegisterHandler<float> OutValue(Context);

    for ( int32 i = 0; i < Context.NumInstances; ++i )
    {
	float row = RowParam.Get();
	FString colTitle = ColTitleParam.Get();
	
	float value = 0.0f;
	if ( CSVFile )
	    CSVFile->GetCSVFloatValue( row, colTitle, value );

	*OutValue.GetDest() = value;
	RowParam.Advance();
	ColTitleParam.Advance();
	OutValue.Advance();
    }
}

template<typename NParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetCSVPosition(FVectorVMContext& Context)
{
    NParamType NParam(Context);
    FRegisterHandler<float> OutSampleX(Context);
    FRegisterHandler<float> OutSampleY(Context);
    FRegisterHandler<float> OutSampleZ(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
	float N = NParam.Get();

	FVector V = FVector::ZeroVector;
	if ( CSVFile )
	    CSVFile->GetCSVPositionValue( N, V );

	*OutSampleX.GetDest() = V.X;
	*OutSampleY.GetDest() = V.Y;
	*OutSampleZ.GetDest() = V.Z;
	NParam.Advance();
	OutSampleX.Advance();
	OutSampleY.Advance();
	OutSampleZ.Advance();
    }
}

template<typename NParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetCSVNormal(FVectorVMContext& Context)
{
    NParamType NParam(Context);
    FRegisterHandler<float> OutSampleX(Context);
    FRegisterHandler<float> OutSampleY(Context);
    FRegisterHandler<float> OutSampleZ(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
	float N = NParam.Get();

	FVector V = FVector::ZeroVector;
	if ( CSVFile )
	    CSVFile->GetCSVNormalValue( N, V );

	*OutSampleX.GetDest() = V.X;
	*OutSampleY.GetDest() = V.Y;
	*OutSampleZ.GetDest() = V.Z;
	NParam.Advance();
	OutSampleX.Advance();
	OutSampleY.Advance();
	OutSampleZ.Advance();
    }
}

template<typename NParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetCSVTime(FVectorVMContext& Context)
{
    NParamType NParam(Context);
    FRegisterHandler<float> OutValue(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
	float N = NParam.Get();

	float value = 0.0f;
	if ( CSVFile )
	    CSVFile->GetCSVTimeValue( N, value );

	*OutValue.GetDest() = value;
	NParam.Advance();
	OutValue.Advance();
    }
}

// Returns the last index of the particles that should be spawned at time t
template<typename TimeParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetLastParticleIndexAtTime(FVectorVMContext& Context)
{
    TimeParamType TimeParam(Context);
    FRegisterHandler<int32> OutValue(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
	float t = TimeParam.Get();

	int32 value = 0;
	if ( CSVFile )
	    CSVFile->GetLastParticleIndexAtTime( t, value );

	*OutValue.GetDest() = value;
	TimeParam.Advance();
	OutValue.Advance();
    }
}

// Returns the last index of the particles that should be spawned at time t
template<typename TimeParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetParticleIndexesToSpawnAtTime( FVectorVMContext& Context )
{
    TimeParamType TimeParam( Context );
    FRegisterHandler<int32> OutMinValue( Context );
    FRegisterHandler<int32> OutMaxValue( Context );
    FRegisterHandler<int32> OutCountValue( Context );

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
	float t = TimeParam.Get();

	int32 value = 0;
	int32 min = 0, max = 0, count = 0;
	if ( CSVFile )
	{
	    if ( !CSVFile->GetLastParticleIndexAtTime( t, value ) )
	    {
		// The CSV file doesn't have time informations, so always return all points in the file
		min = 0;
		max = value;
		count = max - min + 1;
	    }
	    else
	    {
		// The CSV file has time informations
		// First, detect if we need to reset LastSpawnIndex (after a loop)
		if ( value < LastSpawnIndex )
		    LastSpawnIndex = -1;

		if ( value < 0 )
		{
		    // Nothing to spawn, t is lower than the particle's time
		    LastSpawnIndex = -1;
		}
		else
		{
		    // The last time value in the CSV is lower than t, spawn everything if we didnt already!
		    if ( value >= CSVFile->GetNumberOfPointsInCSV() )
			value = value - 1;

		    if ( value == LastSpawnIndex )
		    {
			// We've already spawned all the particles for this time
			min = value;
			max = value;
			count = 0;
		    }
		    else
		    {
			// Found particles to spawn at time t
			min = LastSpawnIndex + 1;
			max = value;
			count = max - min + 1;

			LastSpawnIndex = max;
		    }
		}
	    }
	}

	*OutMinValue.GetDest() = min;
	*OutMaxValue.GetDest() = max;
	*OutCountValue.GetDest() = count;

	TimeParam.Advance();
	OutMinValue.Advance();
	OutMaxValue.Advance();
	OutCountValue.Advance();
    }
}

template<typename NParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetCSVPositionAndTime(FVectorVMContext& Context)
{
    NParamType NParam(Context);

    FRegisterHandler<float> OutPosX(Context);
    FRegisterHandler<float> OutPosY(Context);
    FRegisterHandler<float> OutPosZ(Context);
    FRegisterHandler<float> OutTime(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
	float N = NParam.Get();

	float timeValue = 0.0f;
	FVector posVector = FVector::ZeroVector;
	if ( CSVFile )
	{
	    CSVFile->GetCSVTimeValue(N, timeValue);
	    CSVFile->GetCSVPositionValue(N, posVector);
	}

	*OutPosX.GetDest() = posVector.X;
	*OutPosY.GetDest() = posVector.Y;
	*OutPosZ.GetDest() = posVector.Z;

	*OutTime.GetDest() = timeValue;

	NParam.Advance();
	OutPosX.Advance();
	OutPosY.Advance();
	OutPosZ.Advance();
	OutTime.Advance();
    }
}

void UNiagaraDataInterfaceHoudiniCSV::GetNumberOfPointsInCSV(FVectorVMContext& Context)
{
    FRegisterHandler<int32> OutNumPoints(Context);
    *OutNumPoints.GetDest() = CSVFile ? CSVFile->NumberOfLines : 0;
    OutNumPoints.Advance();
}

#undef LOCTEXT_NAMESPACE
