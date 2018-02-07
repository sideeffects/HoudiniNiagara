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

#include "NiagaraDataInterfaceCSV.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveFloat.h"
#include "NiagaraTypes.h"
#include "Misc/FileHelper.h"

UNiagaraDataInterfaceCSV::UNiagaraDataInterfaceCSV(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
    UpdateDataFromCSVFile();
}

void UNiagaraDataInterfaceCSV::PostInitProperties()
{
    Super::PostInitProperties();

    if (HasAnyFlags(RF_ClassDefaultObject))
    {
	    FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), true, false, false);
    }

    UpdateDataFromCSVFile();
}

void UNiagaraDataInterfaceCSV::PostLoad()
{
    Super::PostLoad();
    UpdateDataFromCSVFile();
}

#if WITH_EDITOR

void UNiagaraDataInterfaceCSV::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceCSV, CSVFileName))
    {
	Modify();		
	UpdateDataFromCSVFile();
    }
}

#endif

bool UNiagaraDataInterfaceCSV::CopyToInternal(UNiagaraDataInterface* Destination) const
{
    if (!Super::CopyToInternal(Destination))
    {
	return false;
    }

    UNiagaraDataInterfaceCSV* CastedInterface = CastChecked<UNiagaraDataInterfaceCSV>(Destination);
    if (!CastedInterface)
	return false;

    CastedInterface->CSVFileName = CSVFileName;
    CastedInterface->UpdateDataFromCSVFile();

    return true;
}

bool UNiagaraDataInterfaceCSV::Equals(const UNiagaraDataInterface* Other) const
{
    if ( !Super::Equals(Other) )
    {
	    return false;
    }

    if ( CastChecked<UNiagaraDataInterfaceCSV>(Other) != NULL )
	return CastChecked<UNiagaraDataInterfaceCSV>(Other)->CSVFileName.FilePath.Equals( CSVFileName.FilePath );

    return false;
}


void UNiagaraDataInterfaceCSV::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
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

    /*
    {
	// GetCSVPositionAndTime
	FNiagaraFunctionSignature Sig;
	Sig.Name = TEXT("GetCSVPositionAndTime");
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));		// CSV in
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("N")));		// Point Number In
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Value")));		// Vector4 Out

	OutFunctions.Add(Sig);
    }
    */

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
}


void UNiagaraDataInterfaceCSV::UpdateDataFromCSVFile()
{
    if ( CSVFileName.FilePath.IsEmpty() )
	return;
    
    // Check the CSV file exists
    const FString FullCSVFilename = FPaths::ConvertRelativePathToFull( CSVFileName.FilePath);
    if ( !FPaths::FileExists( FullCSVFilename ) )
	return;

    // Read each CSV line to its own FString
    TArray<FString> AllLines;
    if (!FFileHelper::LoadANSITextFileToStrings(*FullCSVFilename, NULL, AllLines))
	return;

    // Remove empty strings
    AllLines.RemoveAll([&](const FString& InString) { return InString.IsEmpty(); });
    if ( AllLines.Num() <= 0 )
	return;

    NumberOfColumns = 0;
    NumberOfRows = 0;

    // Number of lines in the CSV (ignoring the title line)
    NumberOfRows = AllLines.Num() - 1;

    // Get the number of values per lines via the title line
    FString TitleString = AllLines[0];
    TitleRowArray.Empty();
    TitleString.ParseIntoArray( TitleRowArray, TEXT(","));
    if ( TitleRowArray.Num() <= 0 )
	return;

    NumberOfColumns = TitleRowArray.Num();

    // Look for packed attributes in the title row
    // Also find the position, normal and time attribute positions
    bool HasPackedPositions = false;
    int32 PositionIndex = -1;
    bool HasPackedNormals = false;
    int32 NormalIndex = -1;
    int32 TimeIndex = -1;

    for ( int32 n = 0; n < TitleRowArray.Num(); n++ )
    {
	if ( TitleRowArray[n].Equals(TEXT("P"), ESearchCase::IgnoreCase) )
	{
	    HasPackedPositions = true;
	    NumberOfColumns += 2;
	    PositionIndex = n;
	}
	else if ( TitleRowArray[n].Equals(TEXT("N"), ESearchCase::IgnoreCase) )
	{
	    HasPackedNormals = true;
	    NumberOfColumns += 2;
	    NormalIndex = n;
	}
	else if ( TitleRowArray[n].Equals(TEXT("Px"), ESearchCase::IgnoreCase) || TitleRowArray[n].Equals(TEXT("X"), ESearchCase::IgnoreCase))
	{
	    HasPackedPositions = false;
	    PositionIndex = n;
	}
	else if ( TitleRowArray[n].Equals(TEXT("Nx"), ESearchCase::IgnoreCase) )
	{
	    HasPackedNormals = false;
	    NormalIndex = n;
	}
	else if ( ( TitleRowArray[n].Equals(TEXT("T"), ESearchCase::IgnoreCase) )
	    || ( TitleRowArray[n].Contains(TEXT("time"), ESearchCase::IgnoreCase) ) )
	{
	    TimeIndex = n;
	}
    }

    // If we have packed positions or normals, we might have to offset other indexes
    if ( HasPackedPositions )
    {
	if ( NormalIndex > PositionIndex )  
	    NormalIndex += 2;

	if ( TimeIndex > PositionIndex )
	    TimeIndex += 2;
    }

    if ( HasPackedNormals )
    {
	if ( PositionIndex > NormalIndex )
	    PositionIndex += 2;

	if ( TimeIndex > NormalIndex )
	    TimeIndex += 2;
    }

    if ( ( NumberOfRows <= 0 ) || ( NumberOfColumns <= 0 ) )
	return;

    // Initialize our different buffers
    CSVData.Empty();
    CSVData.SetNumZeroed(NumberOfRows * NumberOfColumns);

    PositionData.Empty();
    if ( PositionIndex >= 0 )
	PositionData.SetNumZeroed( NumberOfRows * 3 );

    NormalData.Empty();
    if ( NormalIndex >= 0 )
	NormalData.SetNumZeroed( NumberOfRows * 3 );

    TimeData.Empty();
    if ( TimeIndex >= 0 )
	TimeData.SetNumZeroed( NumberOfRows );

    // Extract all the values from the table to the float buffer
    TArray<FString> CurrentParsedLine;
    for ( int rowIdx = 1; rowIdx <= NumberOfRows; rowIdx++ )
    {
	FString CurrentLine = AllLines[ rowIdx ];
	if ( HasPackedNormals || HasPackedPositions )
	{
	    // Clean up the packing "( and )" from the line so it can be parsed properly
	    CurrentLine.ReplaceInline(TEXT("\"("), TEXT(""));
	    CurrentLine.ReplaceInline(TEXT(")\""), TEXT(""));
	}

	// Parse the current line to an array
	CurrentParsedLine.Empty();
	CurrentLine.ParseIntoArray( CurrentParsedLine, TEXT(",") );

	//----------------------------------------------------------------------------
	// FLOAT BUFFER
	for (int colIdx = 0; colIdx < NumberOfColumns; colIdx++)
	{
	    // Convert the string value to a float in the buffer
	    FString CurrentVal = CurrentParsedLine[ colIdx ];
	    CSVData[ ( rowIdx - 1 ) + ( colIdx * NumberOfRows ) ] = FCString::Atof( *CurrentVal );
	}

	//----------------------------------------------------------------------------
	// POSITION BUFFER
	if ( (PositionIndex >= 0) && ( (PositionIndex + 2) < CurrentParsedLine.Num() ) )
	{
	    // Convert the Houdini values to Unreal (Swap Y and Z and scale by a 100)
	    PositionData[rowIdx - 1] = FCString::Atof(*CurrentParsedLine[PositionIndex]) * 100.0f;
	    PositionData[rowIdx - 1 + NumberOfRows] = FCString::Atof(*CurrentParsedLine[PositionIndex + 2]) * 100.0f;
	    PositionData[rowIdx - 1 + 2 * NumberOfRows] = FCString::Atof(*CurrentParsedLine[PositionIndex + 1]) * 100.0f;
	}

	//----------------------------------------------------------------------------
	// NORMAL BUFFER
	if ( (NormalIndex >= 0) && ( ( NormalIndex + 2 ) < CurrentParsedLine.Num() ) )
	{
	    // Convert the Houdini values to Unreal (Swap Y and Z, no need to scale by a 100)
	    NormalData[rowIdx - 1] = FCString::Atof(*CurrentParsedLine[NormalIndex]);
	    NormalData[rowIdx - 1 + NumberOfRows] = FCString::Atof(*CurrentParsedLine[NormalIndex + 2]);
	    NormalData[rowIdx - 1 + 2 * NumberOfRows] = FCString::Atof(*CurrentParsedLine[NormalIndex + 1]);
	}

	//----------------------------------------------------------------------------
	// TIME BUFFER
	if ( CurrentParsedLine.IsValidIndex(TimeIndex) )
	{  
	    TimeData[rowIdx - 1] = FCString::Atof( *CurrentParsedLine[TimeIndex] );
	}
    }

    // Only dirty the GPU buffer when we have actually put some values in it
    GPUBufferDirty = true;
}

// build the shader function HLSL; function name is passed in, as it's defined per-DI; that way, configuration could change
// the HLSL in the spirit of a static switch
// TODO: need a way to identify each specific function here
// 
bool UNiagaraDataInterfaceCSV::GetFunctionHLSL(const FName& DefinitionFunctionName, FString InstanceFunctionName, TArray<FDIGPUBufferParamDescriptor> &Descriptors, FString &HLSLInterfaceID, FString &OutHLSL)
{
    //FString BufferName = Descriptors[0].BufferParamName;
    //FString SecondBufferName = Descriptors[1].BufferParamName;

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
    /*else if (InstanceFunctionName.Contains(TEXT("GetCSVPositionAndTime")))
    {
	OutHLSL += TEXT("void ") + FunctionName + TEXT("(in float In_N, out float4 Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value.x = ") + BufferName + TEXT("[(int)(In_N) ];");
	OutHLSL += TEXT("\t Out_Value.y = ") + BufferName + TEXT("[(int)(In_N + ") + FString::FromInt(NumberOfRows) + TEXT(") ];");
	OutHLSL += TEXT("\t Out_Value.z = ") + BufferName + TEXT("[(int)(In_N + ( 2 * ") + FString::FromInt(NumberOfRows) + TEXT(") ) ];");
	OutHLSL += TEXT("\t Out_Value.w = ") + SecondBufferName + TEXT("[(int)(In_N) ];");
	OutHLSL += TEXT("\n}\n");
    }*/
    else if (InstanceFunctionName.Contains( TEXT("GetNumberOfPointsInCSV") ) )
    {
	OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("( out int Out_Value ) \n{\n");
	OutHLSL += TEXT("\t Out_Value = ") + FString::FromInt(NumberOfRows) + TEXT(";");
	OutHLSL += TEXT("\n}\n");
    }

    return !OutHLSL.IsEmpty();
}

// build buffer definition hlsl
// 1. Choose a buffer name, add the data interface ID (important!)
// 2. add a DIGPUBufferParamDescriptor to the array argument; that'll be passed on to the FNiagaraShader for binding to a shader param, that can
// then later be found by name via FindDIBufferParam for setting; 
// 3. store buffer declaration hlsl in OutHLSL
// multiple buffers can be defined at once here
//
void UNiagaraDataInterfaceCSV::GetBufferDefinitionHLSL(FString DataInterfaceID, TArray<FDIGPUBufferParamDescriptor> &BufferDescriptors, FString &OutHLSL)
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
void UNiagaraDataInterfaceCSV::SetupBuffers(FDIBufferDescriptorStore &BufferDescriptors)
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
TArray<FNiagaraDataInterfaceBufferData> &UNiagaraDataInterfaceCSV::GetBufferDataArray()
{
    check( IsInRenderingThread() );
    if ( GPUBufferDirty )
    {
	check( GPUBuffers.Num() > 0 );

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

	GPUBufferDirty = false;
    }

    return GPUBuffers;
}


DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceCSV, GetCSVFloatValue);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceCSV, GetCSVPosition);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceCSV, GetCSVNormal);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceCSV, GetCSVTime);
//DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceCSV, GetCSVPositionAndTime);
void UNiagaraDataInterfaceCSV::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
    if (BindingInfo.Name == TEXT("GetCSVFloatValue") && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
    {
	TNDIParamBinder<0, float, TNDIParamBinder<1, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceCSV, GetCSVFloatValue)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if (BindingInfo.Name == TEXT("GetCSVPosition") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
    {
	TNDIParamBinder<0, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceCSV, GetCSVPosition)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if (BindingInfo.Name == TEXT("GetCSVNormal") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
    {
	TNDIParamBinder<0, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceCSV, GetCSVNormal)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if (BindingInfo.Name == TEXT("GetCSVTime") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
    {
	TNDIParamBinder<0, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceCSV, GetCSVTime)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    /*else if (BindingInfo.Name == TEXT("GetCSVPositionAndTime") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 4)
    {
	TNDIParamBinder<0, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceCSV, GetCSVPositionAndTime)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }*/
    else if ( BindingInfo.Name == TEXT("GetNumberOfPointsInCSV") && BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 1 )
    {
	OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceCSV::GetNumberOfPointsInCSV);
    }
    else
    {
	UE_LOG(LogNiagara, Error, TEXT("Could not find data interface external function.\n\tExpected Name: GetCSVFloatValue  Actual Name: %s\n\tExpected Inputs: 1  Actual Inputs: %i\n\tExpected Outputs: 3  Actual Outputs: %i"),
	    *BindingInfo.Name.ToString(), BindingInfo.GetNumInputs(), BindingInfo.GetNumOutputs());
	OutFunc = FVMExternalFunction();
    }
}

template<typename RowParamType, typename ColParamType>
void UNiagaraDataInterfaceCSV::GetCSVFloatValue(FVectorVMContext& Context)
{
    RowParamType RowParam(Context);
    ColParamType ColParam(Context);

    FRegisterHandler<float> OutValue(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
	float row = RowParam.Get();
	float col = ColParam.Get();
	
	float value = 0.0f;
	if ( CSVData.IsValidIndex(row + NumberOfRows * col ) )
	    value = CSVData[ row + NumberOfRows * col ];

	*OutValue.GetDest() = value;
	RowParam.Advance();
	ColParam.Advance();
	OutValue.Advance();
    }
}

template<typename NParamType>
void UNiagaraDataInterfaceCSV::GetCSVPosition(FVectorVMContext& Context)
{
    NParamType NParam(Context);
    FRegisterHandler<float> OutSampleX(Context);
    FRegisterHandler<float> OutSampleY(Context);
    FRegisterHandler<float> OutSampleZ(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
	float N = NParam.Get();

	FVector V = FVector::ZeroVector;
	if (PositionData.IsValidIndex(N)
	    && PositionData.IsValidIndex(N + NumberOfRows)
	    && PositionData.IsValidIndex(N + NumberOfRows * 2))
	    V = FVector(PositionData[N], PositionData[N + NumberOfRows], PositionData[N + NumberOfRows * 2]);

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
void UNiagaraDataInterfaceCSV::GetCSVNormal(FVectorVMContext& Context)
{
    NParamType NParam(Context);
    FRegisterHandler<float> OutSampleX(Context);
    FRegisterHandler<float> OutSampleY(Context);
    FRegisterHandler<float> OutSampleZ(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
	float N = NParam.Get();

	FVector V = FVector::ZeroVector;
	if (NormalData.IsValidIndex(N)
	    && NormalData.IsValidIndex(N + NumberOfRows)
	    && NormalData.IsValidIndex(N + NumberOfRows * 2))
	    V = FVector(NormalData[N], NormalData[N + NumberOfRows], NormalData[N + NumberOfRows * 2]);

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
void UNiagaraDataInterfaceCSV::GetCSVTime(FVectorVMContext& Context)
{
    NParamType NParam(Context);
    FRegisterHandler<float> OutValue(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
	float N = NParam.Get();

	float value = 0.0f;
	if (TimeData.IsValidIndex(N))
	    value = TimeData[N];

	*OutValue.GetDest() = value;
	NParam.Advance();
	OutValue.Advance();
    }
}

/*
template<typename NParamType>
void UNiagaraDataInterfaceCSV::GetCSVPositionAndTime(FVectorVMContext& Context)
{
    NParamType NParam(Context);
    FRegisterHandler<float> OutSampleX(Context);
    FRegisterHandler<float> OutSampleY(Context);
    FRegisterHandler<float> OutSampleZ(Context);
    FRegisterHandler<float> OutSampleW(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
	float N = NParam.Get();

	FVector4 V = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
	if (PositionData.IsValidIndex(N)
	    && PositionData.IsValidIndex(N + NumberOfRows)
	    && PositionData.IsValidIndex(N + NumberOfRows * 2)
	    && TimeData.IsValidIndex(N) )
	    V = FVector4(PositionData[N], PositionData[N + NumberOfRows], PositionData[N + NumberOfRows * 2], TimeData[N]);

	*OutSampleX.GetDest() = V.X;
	*OutSampleY.GetDest() = V.Y;
	*OutSampleZ.GetDest() = V.Z;
	*OutSampleW.GetDest() = V.W;
	NParam.Advance();
	OutSampleX.Advance();
	OutSampleY.Advance();
	OutSampleZ.Advance();
	OutSampleW.Advance();
    }
}
*/

void UNiagaraDataInterfaceCSV::GetNumberOfPointsInCSV(FVectorVMContext& Context)
{
    FRegisterHandler<int32> OutNumPoints(Context);
    *OutNumPoints.GetDest() = NumberOfRows;
    OutNumPoints.Advance();
}

