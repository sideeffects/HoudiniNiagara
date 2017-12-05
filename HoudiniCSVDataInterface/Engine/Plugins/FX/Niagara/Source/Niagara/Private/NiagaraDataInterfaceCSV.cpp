// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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

bool UNiagaraDataInterfaceCSV::CopyTo(UNiagaraDataInterface* Destination) const 
{
    if (!Super::CopyTo(Destination))
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
	return CastChecked<UNiagaraDataInterfaceCSV>(Other)->CSVFileName.Equals( CSVFileName );

    return false;
}


void UNiagaraDataInterfaceCSV::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
    {
	// SampleCSV
	FNiagaraFunctionSignature Sig;
	Sig.Name = TEXT("SampleCSV");
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));		// CSV in
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("N")));		// Point Number In
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));     // Vector3 Out

	OutFunctions.Add(Sig);
    }

    {
	// GetNumberOfPointsInCSV
	FNiagaraFunctionSignature Sig;
	Sig.Name = TEXT("GetNumberOfPointsInCSV");
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("NumberOfPoints")));     // Int Out

	OutFunctions.Add(Sig);
    }
}


void UNiagaraDataInterfaceCSV::UpdateDataFromCSVFile()
{
    // reads the data from the CSV file and updates the buffers
    CSVData.Empty();
    TitleRowArray.Empty();

    NumberOfColumns = 0;
    NumberOfRows = 0;

    if ( !CSVFileName.IsEmpty() )
    {
	// Check the CSV file exists
	const FString FullCSVFilename = FPaths::ConvertRelativePathToFull(CSVFileName);
	if ( !FPaths::FileExists( FullCSVFilename ) )
	    return;

	// Read each CSV line to its own FString
	TArray<FString> AllLines;
	if (!FFileHelper::LoadANSITextFileToStrings(*FullCSVFilename, NULL, AllLines))
	    return;

	if ( AllLines.Num() <= 0 )
	    return;

	// Remove empty strings
	AllLines.RemoveAll([&](const FString& InString) { return InString.IsEmpty(); });

	// Number of lines in the CSV (ignoring the title line)
	NumberOfRows = AllLines.Num() - 1;

	// Get the number of values per lines via the title line
	FString TitleString = AllLines[0];
	TitleString.ParseIntoArray( TitleRowArray, TEXT(","));

	if ( TitleRowArray.Num() <= 0 )
	    return;

	NumberOfColumns = TitleRowArray.Num() - 1;

	// Initialize the buffer	
	CSVData.SetNumZeroed( NumberOfRows * NumberOfColumns );

	// extract the values from the table to the float buffer
	TArray<FString> CurrentParsedLine;
	for ( int rowIdx = 1; rowIdx <= NumberOfRows; rowIdx++ )
	{
	    // Parse the current line to an array
	    CurrentParsedLine.Empty();
	    AllLines[ rowIdx ].ParseIntoArray( CurrentParsedLine, TEXT(",") );

	    for (int colIdx = 1; colIdx <= NumberOfColumns; colIdx++)
	    {
		// Convert the string value to a float in the buffer
		FString CurrentVal = CurrentParsedLine[ colIdx ];
		CSVData[ ( rowIdx - 1 ) + ( ( colIdx - 1 ) * NumberOfRows ) ] = FCString::Atof( *CurrentVal );
	    }
	}

	// Only dirty the GPU buffer when we have actually put some values in it
	GPUBufferDirty = true;
    }
    
/*
    // TODO: Properly read / split values from the CSV file
    NumberOfRows = 4;    
    CSVData.SetNumZeroed(NumberOfRows * 3);

    // For Now, hardcode them
    // First store all X Values
    CSVData[0] = 0.0f;
    CSVData[1] = 1.0f;
    CSVData[2] = 2.0f;
    CSVData[3] = 3.0f;

    // then store all Y Values
    CSVData[NumberOfRows + 0] = 0.0f;
    CSVData[NumberOfRows + 1] = 1.0f;
    CSVData[NumberOfRows + 2] = 2.0f;
    CSVData[NumberOfRows + 3] = 3.0f;

    // Finally store all Z Values
    CSVData[2 * NumberOfRows + 0] = 0.0f;
    CSVData[2 * NumberOfRows + 1] = 1.0f;
    CSVData[2 * NumberOfRows + 2] = 2.0f;
    CSVData[2 * NumberOfRows + 3] = 3.0f;   
*/

}

// build the shader function HLSL; function name is passed in, as it's defined per-DI; that way, configuration could change
// the HLSL in the spirit of a static switch
// TODO: need a way to identify each specific function here
// 
bool UNiagaraDataInterfaceCSV::GetFunctionHLSL(FString FunctionName, TArray<DIGPUBufferParamDescriptor> &Descriptors, FString &HLSLInterfaceID, FString &OutHLSL)
{
    FString BufferName = Descriptors[0].BufferParamName;

    if ( FunctionName.Contains( TEXT( "SampleCSV") ) )
    {
	OutHLSL += TEXT("void ") + FunctionName + TEXT("(in float In_N, out float3 Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value.x = ") + BufferName + TEXT("[(int)(In_N) ];");
	OutHLSL += TEXT("\t Out_Value.y = ") + BufferName + TEXT("[(int)(In_N + ") + FString::FromInt(NumberOfRows) + TEXT(") ];");
	OutHLSL += TEXT("\t Out_Value.z = ") + BufferName + TEXT("[(int)(In_N + ( 2 * ") + FString::FromInt(NumberOfRows) + TEXT(") ) ];");
	OutHLSL += TEXT("\n}\n");
    }
    else if ( FunctionName.Contains( TEXT("GetNumberOfPointsInCSV") ) )
    {
	OutHLSL += TEXT("void ") + FunctionName + TEXT("( out int Out_Value ) \n{\n");
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
void UNiagaraDataInterfaceCSV::GetBufferDefinitionHLSL(FString DataInterfaceID, TArray<DIGPUBufferParamDescriptor> &BufferDescriptors, FString &OutHLSL)
{
    FString BufferName = "CSVData" + DataInterfaceID;
    OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

    BufferDescriptors.Add(DIGPUBufferParamDescriptor(BufferName, 0));		// add a descriptor for shader parameter binding
}

// called after translate, to setup buffers matching the buffer descriptors generated during hlsl translation
// need to do this because the script used during translate is a clone, including its DIs
//
void UNiagaraDataInterfaceCSV::SetupBuffers(TArray<DIGPUBufferParamDescriptor> &BufferDescriptors)
{
    for (DIGPUBufferParamDescriptor &Desc : BufferDescriptors)
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

	FNiagaraDataInterfaceBufferData &GPUBuffer = GPUBuffers[0];
	GPUBuffer.Buffer.Release();
	GPUBuffer.Buffer.Initialize(sizeof(float), NumberOfRows * NumberOfColumns, EPixelFormat::PF_R32_FLOAT);	// always allocate for up to 64 data sets
	uint32 BufferSize = CSVData.Num() * sizeof(float);
	int32 *BufferData = static_cast<int32*>(RHILockVertexBuffer(GPUBuffer.Buffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
	FPlatformMemory::Memcpy(BufferData, CSVData.GetData(), BufferSize);
	RHIUnlockVertexBuffer(GPUBuffer.Buffer.Buffer);
	GPUBufferDirty = false;
    }

    return GPUBuffers;
}


DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceCSV, SampleCSV);
FVMExternalFunction UNiagaraDataInterfaceCSV::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData)
{
    if (BindingInfo.Name == TEXT("SampleCSV") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
    {
	return TNDIParamBinder<0, float, NDI_FUNC_BINDER(UNiagaraDataInterfaceCSV, SampleCSV)>::Bind(this, BindingInfo, InstanceData);
    }
    else if ( BindingInfo.Name == TEXT("GetNumberOfPointsInCSV") && BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 1 )
    {
	return FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceCSV::GetNumberOfPointsInCSV);
    }
    else
    {
	UE_LOG(LogNiagara, Error, TEXT("Could not find data interface external function.\n\tExpected Name: SampleCSV  Actual Name: %s\n\tExpected Inputs: 1  Actual Inputs: %i\n\tExpected Outputs: 3  Actual Outputs: %i"),
	    *BindingInfo.Name.ToString(), BindingInfo.GetNumInputs(), BindingInfo.GetNumOutputs());
	return FVMExternalFunction();
    }
}

template<typename NParamType>
void UNiagaraDataInterfaceCSV::SampleCSV(FVectorVMContext& Context)
{
    //TODO: Create some SIMDable optimized representation of the curve to do this faster.
    NParamType NParam(Context);
    FRegisterHandler<float> OutSampleX(Context);
    FRegisterHandler<float> OutSampleY(Context);
    FRegisterHandler<float> OutSampleZ(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
	float N = NParam.Get();
	//FVector V(CSVData[N], CSVData[N + NumberOfValues], CSVData[N + NumberOfValues * 2]);

	FVector V = FVector::ZeroVector;
	if ( CSVData.IsValidIndex(N) 
	    && CSVData.IsValidIndex(N + NumberOfRows) 
	    && CSVData.IsValidIndex(N + NumberOfRows * 2 ) )
	    V = FVector(CSVData[N], CSVData[N + NumberOfRows], CSVData[N + NumberOfRows * 2]);

	*OutSampleX.GetDest() = V.X;
	*OutSampleY.GetDest() = V.Y;
	*OutSampleZ.GetDest() = V.Z;
	NParam.Advance();
	OutSampleX.Advance();
	OutSampleY.Advance();
	OutSampleZ.Advance();
    }
}

void UNiagaraDataInterfaceCSV::GetNumberOfPointsInCSV(FVectorVMContext& Context)
{
    FRegisterHandler<int32> OutNumPoints(Context);
    *OutNumPoints.GetDest() = NumberOfRows;
    OutNumPoints.Advance();
}

