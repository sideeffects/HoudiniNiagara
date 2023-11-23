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

#include "NiagaraDataInterfaceHoudini.h"

#include "HoudiniPointCache.h"

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CoreMiscDefines.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/Paths.h"
#include "NiagaraRenderer.h"
#include "NiagaraShader.h"
#include "NiagaraTypes.h"
#include "ShaderCompiler.h"
#include "ShaderParameterUtils.h"

#define LOCTEXT_NAMESPACE "HoudiniNiagaraDataInterface"


#if ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION < 1

// Base name for the member variables / buffers for GPU compatibility
const FString UNiagaraDataInterfaceHoudini::NumberOfSamplesBaseName(TEXT("NumberOfSamples_"));
const FString UNiagaraDataInterfaceHoudini::NumberOfAttributesBaseName(TEXT("NumberOfAttributes_"));
const FString UNiagaraDataInterfaceHoudini::NumberOfPointsBaseName(TEXT("NumberOfPoints_"));
const FString UNiagaraDataInterfaceHoudini::FloatValuesBufferBaseName(TEXT("FloatValuesBuffer_"));
const FString UNiagaraDataInterfaceHoudini::SpecialAttributeIndexesBufferBaseName(TEXT("SpecialAttributeIndexesBuffer_"));
const FString UNiagaraDataInterfaceHoudini::SpawnTimesBufferBaseName(TEXT("SpawnTimesBuffer_"));
const FString UNiagaraDataInterfaceHoudini::LifeValuesBufferBaseName(TEXT("LifeValuesBuffer_"));
const FString UNiagaraDataInterfaceHoudini::PointTypesBufferBaseName(TEXT("PointTypesBuffer_"));
const FString UNiagaraDataInterfaceHoudini::MaxNumberOfIndexesPerPointBaseName(TEXT("MaxNumberOfIndexesPerPoint_"));
const FString UNiagaraDataInterfaceHoudini::PointValueIndexesBufferBaseName(TEXT("PointValueIndexesBuffer_"));
const FString UNiagaraDataInterfaceHoudini::LastSpawnedPointIdBaseName(TEXT("LastSpawnedPointId_"));
const FString UNiagaraDataInterfaceHoudini::LastSpawnTimeBaseName(TEXT("LastSpawnTime_"));
const FString UNiagaraDataInterfaceHoudini::LastSpawnTimeRequestBaseName(TEXT("LastSpawnTimeRequest_"));
const FString UNiagaraDataInterfaceHoudini::FunctionIndexToAttributeIndexBufferBaseName(TEXT("FunctionIndexToAttributeIndexBuffer_"));

#else
#include "NiagaraShaderParametersBuilder.h"

struct FNiagaraDataInterfaceParametersCS_Houdini : public FNiagaraDataInterfaceParametersCS
{
	DECLARE_TYPE_LAYOUT(FNiagaraDataInterfaceParametersCS_Houdini, NonVirtual);

	LAYOUT_FIELD(TMemoryImageArray<FMemoryImageName>, FunctionIndexToAttribute);
};

IMPLEMENT_TYPE_LAYOUT(FNiagaraDataInterfaceParametersCS_Houdini);

// Base name for the member variables / buffers for GPU compatibility
const FString UNiagaraDataInterfaceHoudini::NumberOfSamplesBaseName(TEXT("_NumberOfSamples"));
const FString UNiagaraDataInterfaceHoudini::NumberOfAttributesBaseName(TEXT("_NumberOfAttributes"));
const FString UNiagaraDataInterfaceHoudini::NumberOfPointsBaseName(TEXT("_NumberOfPoints"));
const FString UNiagaraDataInterfaceHoudini::FloatValuesBufferBaseName(TEXT("_FloatValuesBuffer"));
const FString UNiagaraDataInterfaceHoudini::SpecialAttributeIndexesBufferBaseName(TEXT("_SpecialAttributeIndexesBuffer"));
const FString UNiagaraDataInterfaceHoudini::SpawnTimesBufferBaseName(TEXT("_SpawnTimesBuffer"));
const FString UNiagaraDataInterfaceHoudini::LifeValuesBufferBaseName(TEXT("_LifeValuesBuffer"));
const FString UNiagaraDataInterfaceHoudini::PointTypesBufferBaseName(TEXT("_PointTypesBuffer"));
const FString UNiagaraDataInterfaceHoudini::MaxNumberOfIndexesPerPointBaseName(TEXT("_MaxNumberOfIndexesPerPoint"));
const FString UNiagaraDataInterfaceHoudini::PointValueIndexesBufferBaseName(TEXT("_PointValueIndexesBuffer"));
const FString UNiagaraDataInterfaceHoudini::LastSpawnedPointIdBaseName(TEXT("_LastSpawnedPointId"));
const FString UNiagaraDataInterfaceHoudini::LastSpawnTimeBaseName(TEXT("_LastSpawnTime"));
const FString UNiagaraDataInterfaceHoudini::LastSpawnTimeRequestBaseName(TEXT("_LastSpawnTimeRequest"));
const FString UNiagaraDataInterfaceHoudini::FunctionIndexToAttributeIndexBufferBaseName(TEXT("_FunctionIndexToAttributeIndexBuffer"));


#endif



// Name of all the functions available in the data interface
static const FName GetFloatValueName("GetFloatValue");
static const FName GetFloatValueByStringName("GetFloatValueByString");
static const FName GetVectorValueName("GetVectorValue");
static const FName GetVectorValueByStringName("GetVectorValueByString");
static const FName GetVectorValueExName("GetVectorValueEx");
static const FName GetVectorValueExByStringName("GetVectorValueExByString");
static const FName GetVector4ValueName("GetVector4Value");
static const FName GetVector4ValueByStringName("GetVector4ValueByString");
static const FName GetQuatValueName("GetQuatValue");
static const FName GetQuatValueByStringName("GetQuatValueByString");

static const FName GetPositionName("GetPosition");
static const FName GetNormalName("GetNormal");
static const FName GetTimeName("GetTime");
static const FName GetVelocityName("GetVelocity");
static const FName GetColorName("GetColor");
static const FName GetImpulseName("GetImpulse");
static const FName GetPositionAndTimeName("GetPositionAndTime");

static const FName GetNumberOfPointsName("GetNumberOfPoints");
static const FName GetNumberOfSamplesName("GetNumberOfSamples");
static const FName GetNumberOfAttributesName("GetNumberOfAttributes");

static const FName GetLastSampleIndexAtTimeName("GetLastSampleIndexAtTime");
static const FName GetPointIDsToSpawnAtTimeName("GetPointIDsToSpawnAtTime");
static const FName GetSampleIndexesForPointAtTimeName("GetSampleIndexesForPointAtTime");

static const FName GetPointPositionAtTimeName("GetPointPositionAtTime");
static const FName GetPointValueAtTimeName("GetPointValueAtTime");
static const FName GetPointValueAtTimeByStringName("GetPointValueAtTimeByString");
static const FName GetPointVectorValueAtTimeName("GetPointVectorValueAtTime");
static const FName GetPointVectorValueAtTimeByStringName("GetPointVectorValueAtTimeByString");
static const FName GetPointVector4ValueAtTimeName("GetPointVector4ValueAtTime");
static const FName GetPointVector4ValueAtTimeByStringName("GetPointVector4ValueAtTimeByString");
static const FName GetPointVectorValueAtTimeExName("GetPointVectorValueAtTimeEx");
static const FName GetPointVectorValueAtTimeExByStringName("GetPointVectorValueAtTimeExByString");
static const FName GetPointQuatValueAtTimeName("GetPointQuatValueAtTime");
static const FName GetPointQuatValueAtTimeByStringName("GetPointQuatValueAtTimeByString");

static const FName GetPointLifeName("GetPointLife");
static const FName GetPointLifeAtTimeName("GetPointLifeAtTime");
static const FName GetPointTypeName("GetPointType");

static const FName GetPointNormalAtTimeName("GetPointNormalAtTime");
static const FName GetPointColorAtTimeName("GetPointColorAtTime");
static const FName GetPointAlphaAtTimeName("GetPointAlphaAtTime");
static const FName GetPointVelocityAtTimeName("GetPointVelocityAtTime");
static const FName GetPointImpulseAtTimeName("GetPointImpulseAtTime");
static const FName GetPointTypeAtTimeName("GetPointTypeAtTime");



UNiagaraDataInterfaceHoudini::UNiagaraDataInterfaceHoudini(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
    HoudiniPointCacheAsset = nullptr;

	Proxy.Reset(new FNiagaraDataInterfaceProxyHoudini());
}

void UNiagaraDataInterfaceHoudini::PostInitProperties()
{
    Super::PostInitProperties();

    if (HasAnyFlags(RF_ClassDefaultObject))
    {
		ENiagaraTypeRegistryFlags RegistryFlags = ENiagaraTypeRegistryFlags::AllowUserVariable 
			| ENiagaraTypeRegistryFlags::AllowSystemVariable
			| ENiagaraTypeRegistryFlags::AllowEmitterVariable
			| ENiagaraTypeRegistryFlags::AllowParameter;		

	    FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), RegistryFlags);

		RegistryFlags |= ENiagaraTypeRegistryFlags::AllowPayload;
		FNiagaraTypeRegistry::Register(FHoudiniEvent::StaticStruct(), RegistryFlags);
    }
}

void UNiagaraDataInterfaceHoudini::PostLoad()
{
    Super::PostLoad();

	MarkRenderDataDirty();
}

#if WITH_EDITOR

void UNiagaraDataInterfaceHoudini::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceHoudini, HoudiniPointCacheAsset))
    {
		Modify();
		if ( HoudiniPointCacheAsset )
		{
			MarkRenderDataDirty(); 
		}
    }
}

#endif

bool UNiagaraDataInterfaceHoudini::CopyToInternal(UNiagaraDataInterface* Destination) const
{
    if ( !Super::CopyToInternal( Destination ) )
		return false;

    UNiagaraDataInterfaceHoudini* CastedInterface = CastChecked<UNiagaraDataInterfaceHoudini>( Destination );
    if ( !CastedInterface )
		return false;

    CastedInterface->HoudiniPointCacheAsset = HoudiniPointCacheAsset;
	CastedInterface->MarkRenderDataDirty(); 

    return true;
}

bool UNiagaraDataInterfaceHoudini::Equals(const UNiagaraDataInterface* Other) const
{
    if ( !Super::Equals(Other) )
		return false;

    const UNiagaraDataInterfaceHoudini* OtherHN = CastChecked<UNiagaraDataInterfaceHoudini>(Other);

    if ( OtherHN != nullptr && OtherHN->HoudiniPointCacheAsset != nullptr && HoudiniPointCacheAsset )
    {
		// Just make sure the two interfaces point to the same file
		return OtherHN->HoudiniPointCacheAsset->FileName.Equals( HoudiniPointCacheAsset->FileName );
    }

    return false;
}

// Returns the signature of all the functions avaialable in the data interface
void UNiagaraDataInterfaceHoudini::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
    {
		// GetFloatValue
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetFloatValueName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("SampleIndex")));		// SampleIndex In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("AttributeIndex")));	// AttributeIndex In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));    	// Float Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetFloatValue",
			"Returns the float value in the point cache for a given Sample Index and Attribute Index.\n" ) );

		OutFunctions.Add( Sig );
    }

    {
		// GetFloatValueByString
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetFloatValueByStringName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));	// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("SampleIndex")));	// SampleIndex In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));	// Float Out

		Sig.FunctionSpecifiers.Add(FName("Attribute"));

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetFloatValueByString",
		"Returns the float value in the point cache for a given Sample Index and Attribute name.\n" ) );

		OutFunctions.Add(Sig);
    }

	{
		// GetVectorValue
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetVectorValueName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("SampleIndex")));		// SampleIndex In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("AttributeIndex")));	// AttributeIndex In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));			// Vector3 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetVectorValue",
			"Returns a Vector3 in the point cache for a given Sample Index and Attribute Index.\nThe returned Vector is converted from Houdini's coordinate system to Unreal's." ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetVectorValueByString
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetVectorValueByStringName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));	// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("SampleIndex")));	// SampleIndex In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));		// Vector3 Out

		Sig.FunctionSpecifiers.Add(FName("Attribute"));

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetVectorValueByString",
			"Returns a Vector3 in the point cache for a given Sample Index and Attribute name.\nThe returned Vector is converted from Houdini's coordinate system to Unreal's."));

		OutFunctions.Add(Sig);
	}

	{
		// GetVectorValueEx
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetVectorValueExName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));	// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("SampleIndex")));	// SampleIndex In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("AttributeIndex"))); // AttributeIndex In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoSwap")));		// DoSwap in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoScale")));	// DoScale in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));		// Vector3 Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetVectorValueEx",
			"Returns a Vector3 in the point cache for a given Sample Index and Attribute Index.\nThe DoSwap parameter indicates if the vector should be converted from Houdini*s coordinate system to Unreal's.\nThe DoScale parameter decides if the Vector value should be converted from meters (Houdini) to centimeters (Unreal)."));

		OutFunctions.Add(Sig);
	}

	{
		// GetVectorValueExByString
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetVectorValueExByStringName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));	// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("SampleIndex")));	// SampleIndex In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoSwap")));		// DoSwap in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoScale")));	// DoScale in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));		// Vector3 Out

		Sig.FunctionSpecifiers.Add(FName("Attribute"));

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetVectorValueExByString",
			"Returns a Vector3 in the point cache for a given Sample Index and Attribute name.\nThe DoSwap parameter indicates if the vector should be converted from Houdini*s coordinate system to Unreal's.\nThe DoScale parameter decides if the Vector value should be converted from meters (Houdini) to centimeters (Unreal)."));

		OutFunctions.Add(Sig);
	}
	
	{
		// GetVector4Value
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetVector4ValueName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("SampleIndex")));		// SampleIndex In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("AttributeIndex")));	// AttributeIndex In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Value")));			// Vector4 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetVectorValue",
			"Returns a Vector4 in the point cache for a given Sample Index and Attribute Index.\nThe returned Vector is converted from Houdini's coordinate system to Unreal's." ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetVector4ValueByString
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetVector4ValueByStringName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));	// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("SampleIndex")));	// SampleIndex In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Value")));		// Vector4 Out

		Sig.FunctionSpecifiers.Add(FName("Attribute"));

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetVectorValueByString",
			"Returns a Vector4 in the point cache for a given Sample Index and Attribute name.\nThe returned Vector is converted from Houdini's coordinate system to Unreal's."));

		OutFunctions.Add(Sig);
	}

	{
		// GetQuatValueEx
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetQuatValueName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));	// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("SampleIndex")));	// SampleIndex In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("AttributeIndex"))); // AttributeIndex In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoHoudiniToUnrealConversion")));		// DoHoudiniToUnrealConversion in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(), TEXT("Value")));		// Quat Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetQuatValue",
			"Returns a Quat in the point cache for a given Sample Index and Attribute Index.\nThe DoHoudiniToUnrealConversion parameter indicates if the vector should be converted from Houdini*s coordinate system to Unreal's."));

		OutFunctions.Add(Sig);
	}

	{
		// GetQuatValueByString
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetQuatValueByStringName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));	// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("SampleIndex")));	// SampleIndex In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoHoudiniToUnrealConversion")));		// DoHoudiniToUnrealConversion in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(), TEXT("Value")));		// Quat Out

		Sig.FunctionSpecifiers.Add(FName("Attribute"));

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetQuatValueByString",
			"Returns a Quat in the point cache for a given Sample Index and Attribute name.\nThe DoHoudiniToUnrealConversion parameter indicates if the quat should be converted from Houdini*s coordinate system to Unreal's."));

		OutFunctions.Add(Sig);
	}

	{
		// GetPosition
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPositionName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));	// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("SampleIndex")));	// SampleIndex In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));	// Vector3 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPosition",
			"Helper function returning the position value for a given sample index in the point cache file.\nThe returned Position vector is converted from Houdini's coordinate system to Unreal's." ) );

		OutFunctions.Add(Sig);
    }

    {
		// GetPositionAndTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPositionAndTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));	// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("SampleIndex")));	// SampleIndex In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));	// Vector3 Out
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		// float Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPositionAndTime",
			"Helper function returning the position and time values for a given Sample Index in the point cache.\nThe returned Position vector is converted from Houdini's coordinate system to Unreal's." ) );

		OutFunctions.Add(Sig);
    }

    {
		// GetNormal
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetNormalName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));	// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("SampleIndex")));	// SampleIndex In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));	// Vector3 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetNormal",
			"Helper function returning the normal value for a given Sample Index in the point cache.\nThe returned Normal vector is converted from Houdini's coordinate system to Unreal's." ) );

		OutFunctions.Add(Sig);
    }

    {
		// GetTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));	// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("SampleIndex")));	// SampleIndex In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		// Float Out

		Sig.SetDescription( LOCTEXT("DataInterfaceHoudini_GetTime",
			"Helper function returning the time value for a given Sample Index in the point cache.\n") );

		OutFunctions.Add(Sig);
    }

	{
		// GetVelocity
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetVelocityName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));	// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("SampleIndex")));	// SampleIndex In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));	// Vector3 Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetVelocity",
			"Helper function returning the velocity value for a given Sample Index in the point cache.\nThe returned velocity vector is converted from Houdini's coordinate system to Unreal's."));

		OutFunctions.Add(Sig);
	}

	{
		// GetColor
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetColorName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));	// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("SampleIndex")));	// SampleIndex In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("Color")));	// Color Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetColor",
			"Helper function returning the color value for a given Sample Index in the point cache."));

		OutFunctions.Add(Sig);
	}

	{
		// GetImpulse
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetImpulseName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));	// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("SampleIndex")));	// SampleIndex In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Impulse")));	// Float Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetImpulse",
			"Helper function returning the impulse value for a given Sample Index in the point cache.\n"));

		OutFunctions.Add(Sig);
	}

    {
		// GetNumberOfPoints
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetNumberOfPointsName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));			// PointCache in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("NumberOfPoints")));  // Int Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetNumberOfPoints",
			"Returns the number of points (with different id values) in the point cache.\n" ) );

		OutFunctions.Add(Sig);
    }

	{
		// GetNumberOfSamples
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetNumberOfSamplesName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add( FNiagaraVariable( FNiagaraTypeDefinition( GetClass()), TEXT("PointCache") ) );			// PointCache in
		Sig.Outputs.Add( FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("NumberOfSamples") ) );	// Int Out
		
		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetNumberOfSamples",
			"Returns the number of samples in the point cache." ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetNumberOfAttributes
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetNumberOfAttributesName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add( FNiagaraVariable( FNiagaraTypeDefinition( GetClass()), TEXT("PointCache") ) );				// PointCache in
		Sig.Outputs.Add( FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("NumberOfAttributes") ) );		// Int Out
		
		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetNumberOfAttributes",
			"Returns the number of attributes in the point cache." ) );

		OutFunctions.Add(Sig);
	}

    {
		// GetLastSampleIndexAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetLastSampleIndexAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable( FNiagaraTypeDefinition(GetClass()), TEXT("PointCache") ) );			// PointCache in
		Sig.Inputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time") ) );				// Time in
		Sig.Outputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("LastSampleIndex") ) );	    // Int Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetLastSampleIndexAtTime",
			"Returns the index of the last sample in the point cache that has a time value lesser or equal to the Time parameter." ) );

		OutFunctions.Add(Sig);
    }

    {
		//GetPointIDsToSpawnAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointIDsToSpawnAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable( FNiagaraTypeDefinition(GetClass()), TEXT("PointCache") ) );		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time") ) );		    // Time in
		Sig.Inputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetFloatDef(), TEXT("LastSpawnTime") ) ); // Emitter state In
		Sig.Inputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetFloatDef(), TEXT("LastSpawnTimeRequest") ) );	// Emitter state In
		Sig.Inputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("LastSpawnedPointID") ) );	// Emitter state In
		Sig.Inputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetBoolDef(), TEXT("ResetSpawnState") ) );	// Emitter state In

		Sig.Outputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("MinID") ) );			// Int Out
		Sig.Outputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("MaxID") ) );			// Int Out
		Sig.Outputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("Count") ) );		    // Int Out
		Sig.Outputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetFloatDef(), TEXT("LastSpawnTime") ) ); // Emitter state Out
		Sig.Outputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetFloatDef(), TEXT("LastSpawnTimeRequest") ) );	// Emitter state Out
		Sig.Outputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("LastSpawnedPointID") ) );	// Emitter state Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointIDsToSpawnAtTime",
			"Returns the count and point IDs of the points that should spawn for a given time value." ) );

		OutFunctions.Add(Sig);
    }

	{
		// GetSampleIndexesForPointAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetSampleIndexesForPointAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));				// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));					// Point Number In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));					// Time in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PreviousSampleIndex")));	// Int Out
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("NextSampleIndex")));		// Int Out
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("PrevWeight")));			// Float Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetSampleIndexesForPointAtTime",
			"Returns the sample indexes for a given point at a given time.\nThe previous index, next index and weight can then be used to Lerp between values.") );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointPositionAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointPositionAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));		// Vector3 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointPositionAtTime",
			"Helper function returning the linearly interpolated position for a given point at a given time.\nThe returned Position vector is converted from Houdini's coordinate system to Unreal's.") );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointValueAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointValueAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In		
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("AttributeIndex")));	 // AttributeIndex In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));		// Float Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointValueAtTime",
			"Returns the linearly interpolated value in the specified attribute for a given point at a given time." ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointValueAtTimeByString
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointValueAtTimeByStringName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In		
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));		// Float Out

		Sig.FunctionSpecifiers.Add(FName("Attribute"));

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetPointValueAtTimeByString",
			"Returns the linearly interpolated value in the specified attribute (by name) for a given point at a given time."));

		OutFunctions.Add(Sig);
	}

	{
		// GetPointVectorValueAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointVectorValueAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point ID In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("AttributeIndex")));	// AttributeIndex In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));			// Vector3 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointVectorValueAtTime",
			"Helper function returning the linearly interpolated Vector value in the specified Attribute Index for a given point at a given time.\nThe returned Vector is converted from Houdini's coordinate system to Unreal's." ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointVectorValueAtTimeByString
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointVectorValueAtTimeByStringName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point ID In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));			// Vector3 Out

		Sig.FunctionSpecifiers.Add(FName("Attribute"));

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetPointVectorValueAtTimeByString",
			"Helper function returning the linearly interpolated Vector value in the specified attribute (by name) for a given point at a given time.\nThe returned Vector is converted from Houdini's coordinate system to Unreal's."));

		OutFunctions.Add(Sig);
	}

	{
		// GetPointVector4ValueAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointVector4ValueAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point ID In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("AttributeIndex")));	// AttributeIndex In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Value")));			// Vector4 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointVector4ValueAtTime",
			"Helper function returning the linearly interpolated Vector4 value in the specified Attribute Index for a given point at a given time.\nThe returned Vector is converted from Houdini's coordinate system to Unreal's." ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointVectorValueAtTimeByString
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointVector4ValueAtTimeByStringName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point ID In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Value")));			// Vector4 Out

		Sig.FunctionSpecifiers.Add(FName("Attribute"));

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetPointVector4ValueAtTimeByString",
			"Helper function returning the linearly interpolated Vector4 value in the specified attribute (by name) for a given point at a given time.\nThe returned Vector is converted from Houdini's coordinate system to Unreal's."));

		OutFunctions.Add(Sig);
	}

	{
		// GetPointVectorValueAtTimeEx
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointVectorValueAtTimeExName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point ID In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("AttributeIndex")));	 // AttributeIndex In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoSwap")));			// DoSwap in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoScale")));		// DoScale in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));			// Vector3 Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetPointVectorValueAtTime",
			"Helper function returning the linearly interpolated Vector value in the specified attribute index for a given point at a given time.\nThe DoSwap parameter indicates if the vector should be converted from Houdini*s coordinate system to Unreal's.\nThe DoScale parameter decides if the Vector value should be converted from meters (Houdini) to centimeters (Unreal)." ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointVectorValueAtTimeExByString
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointVectorValueAtTimeExByStringName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point ID In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoSwap")));			// DoSwap in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoScale")));		// DoScale in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));			// Vector3 Out

		Sig.FunctionSpecifiers.Add(FName("Attribute"));

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetPointVectorValueAtTimeByString",
			"Helper function returning the linearly interpolated Vector value in the specified attribute (by name) for a given point at a given time.\nThe DoSwap parameter indicates if the vector should be converted from Houdini*s coordinate system to Unreal's.\nThe DoScale parameter decides if the Vector value should be converted from meters (Houdini) to centimeters (Unreal)."));

		OutFunctions.Add(Sig);
	}

	{
		// GetPointQuatValueAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointQuatValueAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point ID In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("AttributeIndex")));	 // AttributeIndex In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoHoudiniToUnrealConversion")));		// DoHoudiniToUnrealConversion in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(), TEXT("Value")));			// Quat Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetPointQuatValueAtTime",
			"Helper function returning the linearly interpolated Quat value in the specified attribute index for a given point at a given time.\nThe DoHoudiniToUnrealConversion parameter indicates if the vector4 should be converted from Houdini*s coordinate system to Unreal's.\n" ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointQuatValueAtTimeByString
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointQuatValueAtTimeByStringName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point ID In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoHoudiniToUnrealConversion")));		// DoHoudiniToUnrealConversion in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(), TEXT("Value")));			// Quat Out

		Sig.FunctionSpecifiers.Add(FName("Attribute"));

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetPointQuatValueAtTimeByString",
			"Helper function returning the linearly interpolated Quat value in the specified attribute (by name) for a given point at a given time.\nThe DoHoudiniToUnrealConversion parameter indicates if the vector should be converted from Houdini*s coordinate system to Unreal's.\n"));

		OutFunctions.Add(Sig);
	}

	{
		// GetPointLife
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointLifeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Life")));			// Float Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetPointLife",
			"Helper function returning the life value for a given point when spawned.\nThe life value is either calculated from the alive attribute or is the life attribute at spawn time."));

		OutFunctions.Add(Sig);
	}

	{
		// GetPointLifeAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointLifeAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In		
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Life")));			// Float Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointLifeAtTime",
			"Helper function returning the remaining life for a given point in the point cache at a given time." ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointType
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointTypeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Type")));			// Int Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetPointType",
			"Helper function returning the type value for a given point when spawned.\n"));

		OutFunctions.Add(Sig);
	}

	{
		// GetPointNormalAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointNormalAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));		// Vector3 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointNormalAtTime",
			"Helper function returning the linearly interpolated normal for a given point at a given time.\nThe returned vector is converted from Houdini's coordinate system to Unreal's.") );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointColorAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointColorAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Color")));		    // Vector3 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointColorAtTime",
			"Helper function returning the linearly interpolated color for a given point at a given time.") );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointAlphaAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointAlphaAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Alpha")));		// Float Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointAlphaAtTime",
			"Helper function returning the linearly interpolated alpha for a given point at a given time.") );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointVelocityAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointVelocityAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));	    // Vector3 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointVelocityAtTime",
			"Helper function returning the linearly interpolated velocity for a given point at a given time.\nThe returned vector is converted from Houdini's coordinate system to Unreal's.") );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointImpulseAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointImpulseAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Impulse")));		// Impulse

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointImpulseAtTime",
			"Helper function returning the linearly interpolated impulse for a given point at a given time.") );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointTypeAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointTypeAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PointCache")));		// PointCache in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Type")));		    // Type

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointTypeAtTime",
			"Helper function returning the integer type for a given point at a given time.") );

		OutFunctions.Add(Sig);
	}

}

DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetFloatValue);
DEFINE_NDI_DIRECT_FUNC_BINDER_WITH_PAYLOAD(UNiagaraDataInterfaceHoudini, GetFloatValueByString);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetVectorValue);
DEFINE_NDI_DIRECT_FUNC_BINDER_WITH_PAYLOAD(UNiagaraDataInterfaceHoudini, GetVectorValueByString);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetVectorValueEx);
DEFINE_NDI_DIRECT_FUNC_BINDER_WITH_PAYLOAD(UNiagaraDataInterfaceHoudini, GetVectorValueExByString);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetVector4Value);
DEFINE_NDI_DIRECT_FUNC_BINDER_WITH_PAYLOAD(UNiagaraDataInterfaceHoudini, GetVector4ValueByString);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetQuatValue);
DEFINE_NDI_DIRECT_FUNC_BINDER_WITH_PAYLOAD(UNiagaraDataInterfaceHoudini, GetQuatValueByString);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPosition);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetNormal);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetColor);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetVelocity);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetImpulse);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPositionAndTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetLastSampleIndexAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointIDsToSpawnAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetSampleIndexesForPointAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointPositionAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointValueAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER_WITH_PAYLOAD(UNiagaraDataInterfaceHoudini, GetPointValueAtTimeByString);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointVectorValueAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER_WITH_PAYLOAD(UNiagaraDataInterfaceHoudini, GetPointVectorValueAtTimeByString);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointVectorValueAtTimeEx);
DEFINE_NDI_DIRECT_FUNC_BINDER_WITH_PAYLOAD(UNiagaraDataInterfaceHoudini, GetPointVectorValueAtTimeExByString);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointVector4ValueAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER_WITH_PAYLOAD(UNiagaraDataInterfaceHoudini, GetPointVector4ValueAtTimeByString);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointQuatValueAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER_WITH_PAYLOAD(UNiagaraDataInterfaceHoudini, GetPointQuatValueAtTimeByString);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointLife);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointLifeAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointType);

DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointNormalAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointColorAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointAlphaAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointVelocityAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointImpulseAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointTypeAtTime);

void UNiagaraDataInterfaceHoudini::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
	static const FName NAME_Attribute("Attribute");

	const FVMFunctionSpecifier* AttributeSpecifier = BindingInfo.FindSpecifier(NAME_Attribute);
	bool bAttributeSpecifierRequiredButNotFound = false;

	if (BindingInfo.Name == GetFloatValueName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
    {
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetFloatValue)::Bind(this, OutFunc);
    }
    else if (BindingInfo.Name == GetFloatValueByStringName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
    {
		if (AttributeSpecifier)
		{
			NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetFloatValueByString)::Bind(this, OutFunc, AttributeSpecifier->Value.ToString());
		}
		else
		{
			bAttributeSpecifierRequiredButNotFound = true;
		}
    }
    else if (BindingInfo.Name == GetVectorValueName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
    {
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetVectorValue)::Bind(this, OutFunc);
    }
	else if (BindingInfo.Name == GetVectorValueByStringName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
	{
		if (AttributeSpecifier)
		{
			NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetVectorValueByString)::Bind(this, OutFunc, AttributeSpecifier->Value.ToString());
		}
		else
		{
			bAttributeSpecifierRequiredButNotFound = true;
		}
	}
	else if (BindingInfo.Name == GetVectorValueExName && BindingInfo.GetNumInputs() == 4 && BindingInfo.GetNumOutputs() == 3)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetVectorValueEx)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetVectorValueExByStringName && BindingInfo.GetNumInputs() == 3 && BindingInfo.GetNumOutputs() == 3)
	{
		if (AttributeSpecifier)
		{
			NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetVectorValueExByString)::Bind(this, OutFunc, AttributeSpecifier->Value.ToString());
		}
		else
		{
			bAttributeSpecifierRequiredButNotFound = true;
		}
	}
    else if (BindingInfo.Name == GetVector4ValueName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 4)
    {
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetVector4Value)::Bind(this, OutFunc);
    }
	else if (BindingInfo.Name == GetVector4ValueByStringName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 4)
	{
		if (AttributeSpecifier)
		{
			NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetVector4ValueByString)::Bind(this, OutFunc, AttributeSpecifier->Value.ToString());
		}
		else
		{
			bAttributeSpecifierRequiredButNotFound = true;
		}
	}
	else if (BindingInfo.Name == GetQuatValueName && BindingInfo.GetNumInputs() == 3 && BindingInfo.GetNumOutputs() == 4)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetQuatValue)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetQuatValueByStringName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 4)
	{
		if (AttributeSpecifier)
		{
			NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetQuatValueByString)::Bind(this, OutFunc, AttributeSpecifier->Value.ToString());
		}
		else
		{
			bAttributeSpecifierRequiredButNotFound = true;
		}
	}
	else if (BindingInfo.Name == GetPositionName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
    {
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPosition)::Bind(this, OutFunc);
    }
    else if (BindingInfo.Name == GetNormalName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
    {
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetNormal)::Bind(this, OutFunc);
    }
    else if (BindingInfo.Name == GetTimeName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
    {
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetTime)::Bind(this, OutFunc);
    }
	else if (BindingInfo.Name == GetVelocityName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetVelocity)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetColorName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 4)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetColor)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetImpulseName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetImpulse)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPositionAndTimeName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 4)
    {
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPositionAndTime)::Bind(this, OutFunc);
    }
    else if ( BindingInfo.Name == GetNumberOfPointsName && BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 1 )
    {
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceHoudini::GetNumberOfPoints);
    }
	else if (BindingInfo.Name == GetNumberOfSamplesName && BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 1)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceHoudini::GetNumberOfSamples);
	}
	else if (BindingInfo.Name == GetNumberOfAttributesName && BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 1)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceHoudini::GetNumberOfAttributes);
	}
    else if (BindingInfo.Name == GetLastSampleIndexAtTimeName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
    {
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetLastSampleIndexAtTime)::Bind(this, OutFunc);
    }
    else if (BindingInfo.Name == GetPointIDsToSpawnAtTimeName && BindingInfo.GetNumInputs() == 5 && BindingInfo.GetNumOutputs() == 6)
    {
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointIDsToSpawnAtTime)::Bind(this, OutFunc);
    }
	else if (BindingInfo.Name == GetSampleIndexesForPointAtTimeName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetSampleIndexesForPointAtTime)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointPositionAtTimeName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointPositionAtTime)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointValueAtTimeName && BindingInfo.GetNumInputs() == 3 && BindingInfo.GetNumOutputs() == 1)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointValueAtTime)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointValueAtTimeByStringName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
	{
		if (AttributeSpecifier)
		{
			NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointValueAtTimeByString)::Bind(this, OutFunc, AttributeSpecifier->Value.ToString());
		}
		else
		{
			bAttributeSpecifierRequiredButNotFound = true;
		}
	}
	else if (BindingInfo.Name == GetPointVectorValueAtTimeName && BindingInfo.GetNumInputs() == 3 && BindingInfo.GetNumOutputs() == 3)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointVectorValueAtTime)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointVectorValueAtTimeByStringName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
	{
		if (AttributeSpecifier)
		{
			NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointVectorValueAtTimeByString)::Bind(this, OutFunc, AttributeSpecifier->Value.ToString());
		}
		else
		{
			bAttributeSpecifierRequiredButNotFound = true;
		}
	}
	else if (BindingInfo.Name == GetPointVector4ValueAtTimeName && BindingInfo.GetNumInputs() == 3 && BindingInfo.GetNumOutputs() == 4)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointVector4ValueAtTime)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointVector4ValueAtTimeByStringName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 4)
	{
		if (AttributeSpecifier)
		{
			NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointVector4ValueAtTimeByString)::Bind(this, OutFunc, AttributeSpecifier->Value.ToString());
		}
		else
		{
			bAttributeSpecifierRequiredButNotFound = true;
		}
	}
	else if (BindingInfo.Name == GetPointVectorValueAtTimeExName && BindingInfo.GetNumInputs() == 5 && BindingInfo.GetNumOutputs() == 3)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointVectorValueAtTimeEx)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointVectorValueAtTimeExByStringName && BindingInfo.GetNumInputs() == 4 && BindingInfo.GetNumOutputs() == 3)
	{
		if (AttributeSpecifier)
		{
			NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointVectorValueAtTimeExByString)::Bind(this, OutFunc, AttributeSpecifier->Value.ToString());
		}
		else
		{
			bAttributeSpecifierRequiredButNotFound = true;
		}
	}
	else if (BindingInfo.Name == GetPointQuatValueAtTimeName && BindingInfo.GetNumInputs() == 4 && BindingInfo.GetNumOutputs() == 4)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointQuatValueAtTime)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointQuatValueAtTimeByStringName && BindingInfo.GetNumInputs() == 3 && BindingInfo.GetNumOutputs() == 4)
	{
		if (AttributeSpecifier)
		{
			NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointQuatValueAtTimeByString)::Bind(this, OutFunc, AttributeSpecifier->Value.ToString());
		}
		else
		{
			bAttributeSpecifierRequiredButNotFound = true;
		}
	}
	else if (BindingInfo.Name == GetPointLifeName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointLife)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointLifeAtTimeName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointLifeAtTime)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointTypeName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointType)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointNormalAtTimeName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointNormalAtTime)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointColorAtTimeName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointColorAtTime)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointAlphaAtTimeName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointAlphaAtTime)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointVelocityAtTimeName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointVelocityAtTime)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointImpulseAtTimeName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointImpulseAtTime)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointTypeAtTimeName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudini, GetPointTypeAtTime)::Bind(this, OutFunc);
	}
    else
    {
		UE_LOG( LogHoudiniNiagara, Error, 
	    TEXT( "Could not find data interface function:\n\tName: %s\n\tInputs: %i\n\tOutputs: %i" ),
	    *BindingInfo.Name.ToString(), BindingInfo.GetNumInputs(), BindingInfo.GetNumOutputs() );
		OutFunc = FVMExternalFunction();
    }

	if (bAttributeSpecifierRequiredButNotFound)
	{
		// Attribute specifier was required but was not found
		UE_LOG(
			LogHoudiniNiagara, Error,
			TEXT("Could not find specifier '%s' on function:\n\tName: %s\n\tInputs: %i\n\tOutputs: %i"),
			*NAME_Attribute.ToString(), *BindingInfo.Name.ToString(), BindingInfo.GetNumInputs(), BindingInfo.GetNumOutputs()
		);
	}
}

void UNiagaraDataInterfaceHoudini::GetFloatValue(FVectorVMExternalFunctionContext& Context)
{
    VectorVM::FExternalFuncInputHandler<int32> SampleIndexParam(Context);
    VectorVM::FExternalFuncInputHandler<int32> AttributeIndexParam(Context);

    VectorVM::FExternalFuncRegisterHandler<float> OutValue(Context);

    for ( int32 i = 0; i < Context.GetNumInstances(); ++i )
    {
		int32 SampleIndex = SampleIndexParam.Get();
		int32 AttributeIndex = AttributeIndexParam.Get();
	
		float value = 0.0f;
		if ( HoudiniPointCacheAsset )
			HoudiniPointCacheAsset->GetFloatValue( SampleIndex, AttributeIndex, value );

		*OutValue.GetDest() = value;
		SampleIndexParam.Advance();
		AttributeIndexParam.Advance();
		OutValue.Advance();
    }
}

void UNiagaraDataInterfaceHoudini::GetVectorValue(FVectorVMExternalFunctionContext& Context)
{
    VectorVM::FExternalFuncInputHandler<int32> SampleIndexParam(Context);
    VectorVM::FExternalFuncInputHandler<int32> AttributeIndexParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutVectorX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorZ(Context);

    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
		int32 SampleIndex = SampleIndexParam.Get();
		int32 AttributeIndex = AttributeIndexParam.Get();

		FVector V = FVector::ZeroVector;
		if ( HoudiniPointCacheAsset )
			HoudiniPointCacheAsset->GetVectorValue(SampleIndex, AttributeIndex, V);

		*OutVectorX.GetDest() = V.X;
		*OutVectorY.GetDest() = V.Y;
		*OutVectorZ.GetDest() = V.Z;

		SampleIndexParam.Advance();
		AttributeIndexParam.Advance();
		OutVectorX.Advance();
		OutVectorY.Advance();
		OutVectorZ.Advance();
    }
}

void UNiagaraDataInterfaceHoudini::GetVectorValueByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute)
{
	VectorVM::FExternalFuncInputHandler<int32> SampleIndexParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutVectorX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorZ(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 SampleIndex = SampleIndexParam.Get();

		FVector V = FVector::ZeroVector;
		if (HoudiniPointCacheAsset)
			HoudiniPointCacheAsset->GetVectorValueForString(SampleIndex, Attribute, V);

		*OutVectorX.GetDest() = V.X;
		*OutVectorY.GetDest() = V.Y;
		*OutVectorZ.GetDest() = V.Z;

		SampleIndexParam.Advance();
		OutVectorX.Advance();
		OutVectorY.Advance();
		OutVectorZ.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetVectorValueEx(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> SampleIndexParam(Context);
	VectorVM::FExternalFuncInputHandler<int32> AttributeIndexParam(Context);
	VectorVM::FExternalFuncInputHandler<FNiagaraBool> DoSwapParam(Context);
	VectorVM::FExternalFuncInputHandler<FNiagaraBool> DoScaleParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutVectorX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorZ(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 SampleIndex = SampleIndexParam.Get();
		int32 AttributeIndex = AttributeIndexParam.Get();

		bool DoSwap = DoSwapParam.Get().GetValue();
		bool DoScale = DoScaleParam.Get().GetValue();

		FVector V = FVector::ZeroVector;
		if (HoudiniPointCacheAsset)
			HoudiniPointCacheAsset->GetVectorValue(SampleIndex, AttributeIndex, V, DoSwap, DoScale);

		*OutVectorX.GetDest() = V.X;
		*OutVectorY.GetDest() = V.Y;
		*OutVectorZ.GetDest() = V.Z;

		SampleIndexParam.Advance();
		AttributeIndexParam.Advance();
		DoSwapParam.Advance();
		DoScaleParam.Advance();
		OutVectorX.Advance();
		OutVectorY.Advance();
		OutVectorZ.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetVectorValueExByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute)
{
	VectorVM::FExternalFuncInputHandler<int32> SampleIndexParam(Context);
	VectorVM::FExternalFuncInputHandler<FNiagaraBool> DoSwapParam(Context);
	VectorVM::FExternalFuncInputHandler<FNiagaraBool> DoScaleParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutVectorX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorZ(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 SampleIndex = SampleIndexParam.Get();

		bool DoSwap = DoSwapParam.Get().GetValue();
		bool DoScale = DoScaleParam.Get().GetValue();

		FVector V = FVector::ZeroVector;
		if (HoudiniPointCacheAsset)
			HoudiniPointCacheAsset->GetVectorValueForString(SampleIndex, Attribute, V, DoSwap, DoScale);

		*OutVectorX.GetDest() = V.X;
		*OutVectorY.GetDest() = V.Y;
		*OutVectorZ.GetDest() = V.Z;

		SampleIndexParam.Advance();
		DoSwapParam.Advance();
		DoScaleParam.Advance();
		OutVectorX.Advance();
		OutVectorY.Advance();
		OutVectorZ.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetVector4Value(FVectorVMExternalFunctionContext& Context)
{
    VectorVM::FExternalFuncInputHandler<int32> SampleIndexParam(Context);
    VectorVM::FExternalFuncInputHandler<int32> AttributeIndexParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutVectorX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorW(Context);

    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
		int32 SampleIndex = SampleIndexParam.Get();
		int32 AttributeIndex = AttributeIndexParam.Get();

		FVector4 V(FVector::ZeroVector, 0);
		if ( HoudiniPointCacheAsset )
			HoudiniPointCacheAsset->GetVector4Value(SampleIndex, AttributeIndex, V);

		*OutVectorX.GetDest() = V.X;
		*OutVectorY.GetDest() = V.Y;
		*OutVectorZ.GetDest() = V.Z;
		*OutVectorW.GetDest() = V.W;

		SampleIndexParam.Advance();
		AttributeIndexParam.Advance();
		OutVectorX.Advance();
		OutVectorY.Advance();
		OutVectorZ.Advance();
		OutVectorW.Advance();
    }
}

void UNiagaraDataInterfaceHoudini::GetVector4ValueByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute)
{
	VectorVM::FExternalFuncInputHandler<int32> SampleIndexParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutVectorX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorW(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 SampleIndex = SampleIndexParam.Get();

		FVector4 V(FVector::ZeroVector, 0);
		if (HoudiniPointCacheAsset)
			HoudiniPointCacheAsset->GetVector4ValueForString(SampleIndex, Attribute, V);

		*OutVectorX.GetDest() = V.X;
		*OutVectorY.GetDest() = V.Y;
		*OutVectorZ.GetDest() = V.Z;
		*OutVectorW.GetDest() = V.W;

		SampleIndexParam.Advance();
		OutVectorX.Advance();
		OutVectorY.Advance();
		OutVectorZ.Advance();
		OutVectorW.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetQuatValue(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> SampleIndexParam(Context);
	VectorVM::FExternalFuncInputHandler<int32> AttributeIndexParam(Context);
	VectorVM::FExternalFuncInputHandler<FNiagaraBool> DoHoudiniToUnrealConversionParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutVectorX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorW(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 SampleIndex = SampleIndexParam.Get();
		int32 AttributeIndex = AttributeIndexParam.Get();

		bool DoHoudiniToUnrealConversion = DoHoudiniToUnrealConversionParam.Get().GetValue();

		FQuat Q(0, 0, 0, 0);
		if (HoudiniPointCacheAsset)
			HoudiniPointCacheAsset->GetQuatValue(SampleIndex, AttributeIndex, Q, DoHoudiniToUnrealConversion);

		*OutVectorX.GetDest() = Q.X;
		*OutVectorY.GetDest() = Q.Y;
		*OutVectorZ.GetDest() = Q.Z;
		*OutVectorW.GetDest() = Q.W;

		SampleIndexParam.Advance();
		AttributeIndexParam.Advance();
		DoHoudiniToUnrealConversionParam.Advance();
		OutVectorX.Advance();
		OutVectorY.Advance();
		OutVectorZ.Advance();
		OutVectorW.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetQuatValueByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute)
{
	VectorVM::FExternalFuncInputHandler<int32> SampleIndexParam(Context);
	VectorVM::FExternalFuncInputHandler<FNiagaraBool> DoHoudiniToUnrealConversionParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutVectorX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorW(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 SampleIndex = SampleIndexParam.Get();

		bool DoHoudiniToUnrealConversion = DoHoudiniToUnrealConversionParam.Get().GetValue();

		FQuat Q(0, 0, 0, 0);
		if (HoudiniPointCacheAsset)
			HoudiniPointCacheAsset->GetQuatValueForString(SampleIndex, Attribute, Q, DoHoudiniToUnrealConversion);

		*OutVectorX.GetDest() = Q.X;
		*OutVectorY.GetDest() = Q.Y;
		*OutVectorZ.GetDest() = Q.Z;
		*OutVectorW.GetDest() = Q.W;

		SampleIndexParam.Advance();
		DoHoudiniToUnrealConversionParam.Advance();
		OutVectorX.Advance();
		OutVectorY.Advance();
		OutVectorZ.Advance();
		OutVectorW.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetFloatValueByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute)
{
    VectorVM::FExternalFuncInputHandler<int32> SampleIndexParam(Context);

    VectorVM::FExternalFuncRegisterHandler<float> OutValue(Context);

    for ( int32 i = 0; i < Context.GetNumInstances(); ++i )
    {
		int32 SampleIndex = SampleIndexParam.Get();
	
		float value = 0.0f;
		if (HoudiniPointCacheAsset)
			HoudiniPointCacheAsset->GetFloatValueForString(SampleIndex, Attribute, value);

		*OutValue.GetDest() = value;
		SampleIndexParam.Advance();
		OutValue.Advance();
    }
}

void UNiagaraDataInterfaceHoudini::GetPosition(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> SampleIndexParam(Context);

    VectorVM::FExternalFuncRegisterHandler<float> OutSampleX(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutSampleY(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutSampleZ(Context);

    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
		int32 SampleIndex = SampleIndexParam.Get();

		FVector V = FVector::ZeroVector;
		if ( HoudiniPointCacheAsset )
			HoudiniPointCacheAsset->GetPositionValue( SampleIndex, V );

		*OutSampleX.GetDest() = V.X;
		*OutSampleY.GetDest() = V.Y;
		*OutSampleZ.GetDest() = V.Z;
		SampleIndexParam.Advance();
		OutSampleX.Advance();
		OutSampleY.Advance();
		OutSampleZ.Advance();
    }
}

void UNiagaraDataInterfaceHoudini::GetNormal(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> SampleIndexParam(Context);

    VectorVM::FExternalFuncRegisterHandler<float> OutSampleX(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutSampleY(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutSampleZ(Context);

    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
		int32 SampleIndex = SampleIndexParam.Get();

		FVector V = FVector::ZeroVector;
		if ( HoudiniPointCacheAsset )
			HoudiniPointCacheAsset->GetNormalValue( SampleIndex, V );

		*OutSampleX.GetDest() = V.X;
		*OutSampleY.GetDest() = V.Y;
		*OutSampleZ.GetDest() = V.Z;
		SampleIndexParam.Advance();
		OutSampleX.Advance();
		OutSampleY.Advance();
		OutSampleZ.Advance();
    }
}

void UNiagaraDataInterfaceHoudini::GetTime(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> SampleIndexParam(Context);

    VectorVM::FExternalFuncRegisterHandler<float> OutValue(Context);

    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
		int32 SampleIndex = SampleIndexParam.Get();

		float value = 0.0f;
		if ( HoudiniPointCacheAsset )
			HoudiniPointCacheAsset->GetTimeValue( SampleIndex, value );

		*OutValue.GetDest() = value;
		SampleIndexParam.Advance();
		OutValue.Advance();
    }
}

void UNiagaraDataInterfaceHoudini::GetVelocity(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> SampleIndexParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutSampleX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleZ(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 SampleIndex = SampleIndexParam.Get();

		FVector V = FVector::ZeroVector;
		if ( HoudiniPointCacheAsset )
			HoudiniPointCacheAsset->GetVelocityValue( SampleIndex, V );

		*OutSampleX.GetDest() = V.X;
		*OutSampleY.GetDest() = V.Y;
		*OutSampleZ.GetDest() = V.Z;
		SampleIndexParam.Advance();
		OutSampleX.Advance();
		OutSampleY.Advance();
		OutSampleZ.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetColor(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> SampleIndexParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutSampleR(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleG(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleB(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleA(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 SampleIndex = SampleIndexParam.Get();

		FLinearColor C = FLinearColor::White;
		if ( HoudiniPointCacheAsset )
			HoudiniPointCacheAsset->GetColorValue( SampleIndex, C );

		*OutSampleR.GetDest() = C.R;
		*OutSampleG.GetDest() = C.G;
		*OutSampleB.GetDest() = C.B;
		*OutSampleA.GetDest() = C.A;
		SampleIndexParam.Advance();
		OutSampleR.Advance();
		OutSampleG.Advance();
		OutSampleB.Advance();
		OutSampleA.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetImpulse(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> SampleIndexParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutValue(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 SampleIndex = SampleIndexParam.Get();

		float value = 0.0f;
		if (HoudiniPointCacheAsset)
			HoudiniPointCacheAsset->GetImpulseValue(SampleIndex, value);

		*OutValue.GetDest() = value;
		SampleIndexParam.Advance();
		OutValue.Advance();
	}
}

// Returns the last index of the points that should be spawned at time t
void UNiagaraDataInterfaceHoudini::GetLastSampleIndexAtTime(FVectorVMExternalFunctionContext& Context)
{
    VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);

    VectorVM::FExternalFuncRegisterHandler<int32> OutValue(Context);

    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
		float t = TimeParam.Get();

		int32 value = 0;
		if ( HoudiniPointCacheAsset )
			HoudiniPointCacheAsset->GetLastSampleIndexAtTime( t, value );

		*OutValue.GetDest() = value;
		TimeParam.Advance();
		OutValue.Advance();
    }
}

// Returns the last index of the points that should be spawned at time t
void UNiagaraDataInterfaceHoudini::GetPointIDsToSpawnAtTime(FVectorVMExternalFunctionContext& Context)
{
    VectorVM::FExternalFuncInputHandler<float> TimeParam( Context );
	VectorVM::FExternalFuncInputHandler<float> LastSpawnTimeParam( Context );
	VectorVM::FExternalFuncInputHandler<float> LastSpawnTimeRequestParam( Context );
	VectorVM::FExternalFuncInputHandler<int32> LastSpawnedPointIDParam( Context );
	VectorVM::FExternalFuncInputHandler<bool>  ResetSpawnStateParam( Context );

    VectorVM::FExternalFuncRegisterHandler<int32> OutMinValue( Context );
    VectorVM::FExternalFuncRegisterHandler<int32> OutMaxValue( Context );
    VectorVM::FExternalFuncRegisterHandler<int32> OutCountValue( Context );
	VectorVM::FExternalFuncRegisterHandler<float> OutLastSpawnTimeValue( Context );
	VectorVM::FExternalFuncRegisterHandler<float> OutLastSpawnTimeRequestValue( Context );
	VectorVM::FExternalFuncRegisterHandler<int32> OutLastSpawnedPointIDValue( Context );

    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
		float t = TimeParam.Get();
		float LastSpawnTime = LastSpawnTimeParam.Get();
		float LastSpawnTimeRequest = LastSpawnTimeRequestParam.Get();
		int32 LastSpawnedPointID = LastSpawnedPointIDParam.Get();
		bool  ResetSpawnState = ResetSpawnStateParam.Get();

		if (ResetSpawnState)
		{
			LastSpawnTime = -FLT_MAX;
			LastSpawnTimeRequest = -FLT_MAX;
			LastSpawnedPointID = -1;
		}
		

		int32 value = 0;
		int32 min = 0, max = 0, count = 0;

		if ( HoudiniPointCacheAsset )
		{
			HoudiniPointCacheAsset->GetPointIDsToSpawnAtTime(t, min, max, count, LastSpawnedPointID, LastSpawnTime, LastSpawnTimeRequest);
		}

		*OutMinValue.GetDest() = min;
		*OutMaxValue.GetDest() = max;
		*OutCountValue.GetDest() = count;

		*OutLastSpawnTimeValue.GetDest() = LastSpawnTime;
		*OutLastSpawnTimeRequestValue.GetDest() = LastSpawnTimeRequest;
		*OutLastSpawnedPointIDValue.GetDest() = LastSpawnedPointID;


		TimeParam.Advance();
		OutMinValue.Advance();
		OutMaxValue.Advance();
		OutCountValue.Advance();
		OutLastSpawnTimeValue.Advance();
		OutLastSpawnTimeRequestValue.Advance();
		OutLastSpawnedPointIDValue.Advance();
    }
}

void UNiagaraDataInterfaceHoudini::GetPositionAndTime(FVectorVMExternalFunctionContext& Context)
{
    VectorVM::FExternalFuncInputHandler<int32> SampleIndexParam(Context);

    VectorVM::FExternalFuncRegisterHandler<float> OutPosX(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutPosY(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutPosZ(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutTime(Context);

    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
		int32 SampleIndex = SampleIndexParam.Get();

		float timeValue = 0.0f;
		FVector posVector = FVector::ZeroVector;
		if ( HoudiniPointCacheAsset )
		{
			HoudiniPointCacheAsset->GetTimeValue( SampleIndex, timeValue);
			HoudiniPointCacheAsset->GetPositionValue( SampleIndex, posVector);
		}

		*OutPosX.GetDest() = posVector.X;
		*OutPosY.GetDest() = posVector.Y;
		*OutPosZ.GetDest() = posVector.Z;

		*OutTime.GetDest() = timeValue;

		SampleIndexParam.Advance();
		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
		OutTime.Advance();
    }
}

void UNiagaraDataInterfaceHoudini::GetSampleIndexesForPointAtTime(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutPrevIndex(Context);
	VectorVM::FExternalFuncRegisterHandler<int32> OutNextIndex(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutWeightValue(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
		int32 PointID = PointIDParam.Get();
		float time = TimeParam.Get();

		float weight = 0.0f;
		int32 prevIdx = 0;
		int32 nextIdx = 0;
		if ( HoudiniPointCacheAsset )
		{
			HoudiniPointCacheAsset->GetSampleIndexesForPointAtTime( PointID, time, prevIdx, nextIdx, weight );
		}

		*OutPrevIndex.GetDest() = prevIdx;
		*OutNextIndex.GetDest() = nextIdx;
		*OutWeightValue.GetDest() = weight;

		PointIDParam.Advance();
		TimeParam.Advance();
		OutPrevIndex.Advance();
		OutNextIndex.Advance();
		OutWeightValue.Advance();
    }
}

void UNiagaraDataInterfaceHoudini::GetPointPositionAtTime(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosZ(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
		int32 PointID = PointIDParam.Get();
		float time = TimeParam.Get();

		FVector posVector = FVector::ZeroVector;
		if ( HoudiniPointCacheAsset )
		{
			HoudiniPointCacheAsset->GetPointPositionAtTime(PointID, time, posVector);
		}		

		*OutPosX.GetDest() = posVector.X;
		*OutPosY.GetDest() = posVector.Y;
		*OutPosZ.GetDest() = posVector.Z;

		PointIDParam.Advance();
		TimeParam.Advance();
		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
    }
}

void UNiagaraDataInterfaceHoudini::GetPointValueAtTime(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);
	VectorVM::FExternalFuncInputHandler<int32> AttributeIndexParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutValue(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 PointID = PointIDParam.Get();
		int32 AttrIndex = AttributeIndexParam.Get();
		float time = TimeParam.Get();		

		float Value = 0.0f;
		if ( HoudiniPointCacheAsset )
		{
			HoudiniPointCacheAsset->GetPointValueAtTime( PointID, AttrIndex, time, Value );
		}

		*OutValue.GetDest() = Value;

		PointIDParam.Advance();
		AttributeIndexParam.Advance();
		TimeParam.Advance();

		OutValue.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetPointValueAtTimeByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutValue(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 PointID = PointIDParam.Get();
		float time = TimeParam.Get();

		float Value = 0.0f;
		if (HoudiniPointCacheAsset)
		{
			HoudiniPointCacheAsset->GetPointValueAtTimeForString(PointID, Attribute, time, Value);
		}

		*OutValue.GetDest() = Value;

		PointIDParam.Advance();
		TimeParam.Advance();

		OutValue.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetPointVectorValueAtTime(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<int32> AttributeIndexParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);	

	VectorVM::FExternalFuncRegisterHandler<float> OutPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosZ(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 PointID = PointIDParam.Get();
		int32 AttrIndex = AttributeIndexParam.Get();
		float time = TimeParam.Get();		

		FVector posVector = FVector::ZeroVector;
		if ( HoudiniPointCacheAsset )
		{
			HoudiniPointCacheAsset->GetPointVectorValueAtTime( PointID, AttrIndex, time, posVector, true, true);
		}

		*OutPosX.GetDest() = posVector.X;
		*OutPosY.GetDest() = posVector.Y;
		*OutPosZ.GetDest() = posVector.Z;

		PointIDParam.Advance();
		AttributeIndexParam.Advance();
		TimeParam.Advance();
		
		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetPointVectorValueAtTimeByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosZ(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 PointID = PointIDParam.Get();
		float time = TimeParam.Get();

		FVector posVector = FVector::ZeroVector;
		if (HoudiniPointCacheAsset)
		{
			HoudiniPointCacheAsset->GetPointVectorValueAtTimeForString(PointID, Attribute, time, posVector, true, true);
		}

		*OutPosX.GetDest() = posVector.X;
		*OutPosY.GetDest() = posVector.Y;
		*OutPosZ.GetDest() = posVector.Z;

		PointIDParam.Advance();
		TimeParam.Advance();

		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetPointVectorValueAtTimeEx(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<int32> AttributeIndexParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);
	VectorVM::FExternalFuncInputHandler<FNiagaraBool> DoSwapParam(Context);
	VectorVM::FExternalFuncInputHandler<FNiagaraBool> DoScaleParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosZ(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 PointID = PointIDParam.Get();
		int32 AttrIndex = AttributeIndexParam.Get();
		float time = TimeParam.Get();

		bool DoSwap = DoSwapParam.Get().GetValue();
		bool DoScale = DoScaleParam.Get().GetValue();

		FVector posVector = FVector::ZeroVector;
		if (HoudiniPointCacheAsset)
		{
			HoudiniPointCacheAsset->GetPointVectorValueAtTime(PointID, AttrIndex, time, posVector, DoSwap, DoScale);
		}

		*OutPosX.GetDest() = posVector.X;
		*OutPosY.GetDest() = posVector.Y;
		*OutPosZ.GetDest() = posVector.Z;

		PointIDParam.Advance();
		AttributeIndexParam.Advance();
		TimeParam.Advance();
		DoSwapParam.Advance();
		DoScaleParam.Advance();

		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetPointVectorValueAtTimeExByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);
	VectorVM::FExternalFuncInputHandler<FNiagaraBool> DoSwapParam(Context);
	VectorVM::FExternalFuncInputHandler<FNiagaraBool> DoScaleParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosZ(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 PointID = PointIDParam.Get();
		float time = TimeParam.Get();

		bool DoSwap = DoSwapParam.Get().GetValue();
		bool DoScale = DoScaleParam.Get().GetValue();

		FVector posVector = FVector::ZeroVector;
		if (HoudiniPointCacheAsset)
		{
			HoudiniPointCacheAsset->GetPointVectorValueAtTimeForString(PointID, Attribute, time, posVector, DoSwap, DoScale);
		}

		*OutPosX.GetDest() = posVector.X;
		*OutPosY.GetDest() = posVector.Y;
		*OutPosZ.GetDest() = posVector.Z;

		PointIDParam.Advance();
		TimeParam.Advance();
		DoSwapParam.Advance();
		DoScaleParam.Advance();

		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetPointVector4ValueAtTime(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<int32> AttributeIndexParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);	

	VectorVM::FExternalFuncRegisterHandler<float> OutPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosW(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 PointID = PointIDParam.Get();
		int32 AttrIndex = AttributeIndexParam.Get();
		float time = TimeParam.Get();		

		FVector4 posVector(FVector::ZeroVector, 0);
		if ( HoudiniPointCacheAsset )
		{
			HoudiniPointCacheAsset->GetPointVector4ValueAtTime( PointID, AttrIndex, time, posVector);
		}

		*OutPosX.GetDest() = posVector.X;
		*OutPosY.GetDest() = posVector.Y;
		*OutPosZ.GetDest() = posVector.Z;
		*OutPosW.GetDest() = posVector.W;

		PointIDParam.Advance();
		AttributeIndexParam.Advance();
		TimeParam.Advance();
		
		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
		OutPosW.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetPointVector4ValueAtTimeByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosW(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 PointID = PointIDParam.Get();
		float time = TimeParam.Get();

		FVector4 posVector(FVector::ZeroVector, 0);
		if (HoudiniPointCacheAsset)
		{
			HoudiniPointCacheAsset->GetPointVector4ValueAtTimeForString(PointID, Attribute, time, posVector);
		}

		*OutPosX.GetDest() = posVector.X;
		*OutPosY.GetDest() = posVector.Y;
		*OutPosZ.GetDest() = posVector.Z;
		*OutPosW.GetDest() = posVector.W;

		PointIDParam.Advance();
		TimeParam.Advance();

		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
		OutPosW.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetPointQuatValueAtTime(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<int32> AttributeIndexParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);
	VectorVM::FExternalFuncInputHandler<FNiagaraBool> DoHoudiniToUnrealConversionParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosW(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 PointID = PointIDParam.Get();
		int32 AttrIndex = AttributeIndexParam.Get();
		float time = TimeParam.Get();

		bool DoHoudiniToUnrealConversion = DoHoudiniToUnrealConversionParam.Get().GetValue();

		FQuat Q(0, 0, 0, 0);
		if (HoudiniPointCacheAsset)
		{
			HoudiniPointCacheAsset->GetPointQuatValueAtTime(PointID, AttrIndex, time, Q, DoHoudiniToUnrealConversion);
		}

		*OutPosX.GetDest() = Q.X;
		*OutPosY.GetDest() = Q.Y;
		*OutPosZ.GetDest() = Q.Z;
		*OutPosW.GetDest() = Q.W;

		PointIDParam.Advance();
		AttributeIndexParam.Advance();
		TimeParam.Advance();
		DoHoudiniToUnrealConversionParam.Advance();

		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
		OutPosW.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetPointQuatValueAtTimeByString(FVectorVMExternalFunctionContext& Context, const FString& Attribute)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);
	VectorVM::FExternalFuncInputHandler<FNiagaraBool> DoHoudiniToUnrealConversionParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosW(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 PointID = PointIDParam.Get();
		float time = TimeParam.Get();

		bool DoHoudiniToUnrealConversion = DoHoudiniToUnrealConversionParam.Get().GetValue();

		FQuat Q(0, 0, 0, 0);
		if (HoudiniPointCacheAsset)
		{
			HoudiniPointCacheAsset->GetPointQuatValueAtTimeForString(PointID, Attribute, time, Q, DoHoudiniToUnrealConversion);
		}

		*OutPosX.GetDest() = Q.X;
		*OutPosY.GetDest() = Q.Y;
		*OutPosZ.GetDest() = Q.Z;
		*OutPosW.GetDest() = Q.W;

		PointIDParam.Advance();
		TimeParam.Advance();
		DoHoudiniToUnrealConversionParam.Advance();

		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
		OutPosW.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetPointLife(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutValue(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 PointID = PointIDParam.Get();

		float Value = 0.0f;
		if ( HoudiniPointCacheAsset )
		{
			HoudiniPointCacheAsset->GetPointLife(PointID, Value);
		}

		*OutValue.GetDest() = Value;

		PointIDParam.Advance();

		OutValue.Advance();
	}
}

//template<typename VectorVM::FExternalFuncInputHandler<int32>, typename VectorVM::FExternalFuncInputHandler<float>>
void UNiagaraDataInterfaceHoudini::GetPointLifeAtTime(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutValue(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 PointID = PointIDParam.Get();
		float time = TimeParam.Get();

		float Value = 0.0f;
		if ( HoudiniPointCacheAsset )
		{
			HoudiniPointCacheAsset->GetPointLifeAtTime(PointID, time, Value);
		}

		*OutValue.GetDest() = Value;

		PointIDParam.Advance();
		TimeParam.Advance();

		OutValue.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetPointType(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutValue(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 PointID = PointIDParam.Get();

		int32 Value = 0;
		if (HoudiniPointCacheAsset)
		{
			HoudiniPointCacheAsset->GetPointType(PointID, Value);
		}

		*OutValue.GetDest() = Value;

		PointIDParam.Advance();

		OutValue.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetPointGenericVectorAttributeAtTime(EHoudiniAttributes Attribute, FVectorVMExternalFunctionContext& Context, bool DoSwap, bool DoScale)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutVecX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVecY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVecZ(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 PointID = PointIDParam.Get();
		float Time = TimeParam.Get();

		FVector VectorValue = FVector::ZeroVector;
		if ( HoudiniPointCacheAsset )
		{
			int32 AttrIndex = HoudiniPointCacheAsset->GetAttributeAttributeIndex(Attribute);
			HoudiniPointCacheAsset->GetPointVectorValueAtTime(PointID, AttrIndex, Time, VectorValue, DoSwap, DoScale);
		}

		*OutVecX.GetDest() = VectorValue.X;
		*OutVecY.GetDest() = VectorValue.Y;
		*OutVecZ.GetDest() = VectorValue.Z;

		PointIDParam.Advance();
		TimeParam.Advance();
		OutVecX.Advance();
		OutVecY.Advance();
		OutVecZ.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetPointGenericFloatAttributeAtTime(EHoudiniAttributes Attribute, FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutValue(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 PointID = PointIDParam.Get();
		float Time = TimeParam.Get();

		float Value = 0.0f;
		if ( HoudiniPointCacheAsset )
		{
			int32 AttrIndex = HoudiniPointCacheAsset->GetAttributeAttributeIndex(Attribute);
			HoudiniPointCacheAsset->GetPointFloatValueAtTime(PointID, AttrIndex, Time, Value);
		}

		*OutValue.GetDest() = Value;

		PointIDParam.Advance();
		TimeParam.Advance();
		OutValue.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetPointGenericInt32AttributeAtTime(EHoudiniAttributes Attribute, FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutValue(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 PointID = PointIDParam.Get();
		float Time = TimeParam.Get();

		int32 Value = 0.0f;
		if ( HoudiniPointCacheAsset )
		{
			int32 AttrIndex = HoudiniPointCacheAsset->GetAttributeAttributeIndex(Attribute);
			HoudiniPointCacheAsset->GetPointInt32ValueAtTime(PointID, AttrIndex, Time, Value);
		}

		*OutValue.GetDest() = Value;

		PointIDParam.Advance();
		TimeParam.Advance();
		OutValue.Advance();
	}
}

void UNiagaraDataInterfaceHoudini::GetPointNormalAtTime(FVectorVMExternalFunctionContext& Context)
{
	GetPointGenericVectorAttributeAtTime(EHoudiniAttributes::NORMAL, Context, true, false);
}

void UNiagaraDataInterfaceHoudini::GetPointColorAtTime(FVectorVMExternalFunctionContext& Context)
{
	GetPointGenericVectorAttributeAtTime(EHoudiniAttributes::COLOR, Context, false, false);
}

void UNiagaraDataInterfaceHoudini::GetPointAlphaAtTime(FVectorVMExternalFunctionContext& Context)
{
	GetPointGenericFloatAttributeAtTime(EHoudiniAttributes::ALPHA, Context);
}

void UNiagaraDataInterfaceHoudini::GetPointVelocityAtTime(FVectorVMExternalFunctionContext& Context)
{
	GetPointGenericVectorAttributeAtTime(EHoudiniAttributes::VELOCITY, Context, true, true);
}

void UNiagaraDataInterfaceHoudini::GetPointImpulseAtTime(FVectorVMExternalFunctionContext& Context)
{
	GetPointGenericFloatAttributeAtTime(EHoudiniAttributes::IMPULSE, Context);
}

void UNiagaraDataInterfaceHoudini::GetPointTypeAtTime(FVectorVMExternalFunctionContext& Context)
{
	GetPointGenericInt32AttributeAtTime(EHoudiniAttributes::TYPE, Context);
}


void UNiagaraDataInterfaceHoudini::GetNumberOfSamples(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncRegisterHandler<int32> OutNumSamples(Context);
	*OutNumSamples.GetDest() = HoudiniPointCacheAsset ? HoudiniPointCacheAsset->GetNumberOfSamples() : 0;
	OutNumSamples.Advance();
}

void UNiagaraDataInterfaceHoudini::GetNumberOfAttributes(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncRegisterHandler<int32> OutNumAttributes(Context);
	*OutNumAttributes.GetDest() = HoudiniPointCacheAsset ? HoudiniPointCacheAsset->GetNumberOfAttributes() : 0;
	OutNumAttributes.Advance();
}

void UNiagaraDataInterfaceHoudini::GetNumberOfPoints(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncRegisterHandler<int32> OutNumPoints(Context);
	*OutNumPoints.GetDest() = HoudiniPointCacheAsset ? HoudiniPointCacheAsset->GetNumberOfPoints() : 0;
	OutNumPoints.Advance();
}

bool UNiagaraDataInterfaceHoudini::GetAttributeFunctionIndex(const TArray<FNiagaraDataInterfaceGeneratedFunction>& InGeneratedFunctions, int InFunctionIndex, int &OutAttributeFunctionIndex) const
{
	const uint32 NumGeneratedFunctions = InGeneratedFunctions.Num();
	if (NumGeneratedFunctions == 0 || InFunctionIndex < 0)
		return false;

	const FName NAME_Attribute(TEXT("Attribute"));
	int AttributeFunctionIndex = 0;
	for (uint32 FunctionIndex = 0; FunctionIndex < NumGeneratedFunctions; ++FunctionIndex)
	{
		const FNiagaraDataInterfaceGeneratedFunction& GeneratedFunction = InGeneratedFunctions[FunctionIndex];
		const FName *Attribute = GeneratedFunction.FindSpecifierValue(NAME_Attribute);
		if (Attribute != nullptr)
		{
			if (FunctionIndex == InFunctionIndex)
			{
				OutAttributeFunctionIndex = AttributeFunctionIndex;
				return true;
			}
			else
			{
				AttributeFunctionIndex++;
			}
		}
	}

	return false;
}

#if ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION < 1
#if WITH_EDITORONLY_DATA
void UNiagaraDataInterfaceHoudini::GetCommonHLSL(FString& OutHLSL)
{
	OutHLSL += TEXT("float4 q_slerp(in float4 Quat1, in float4 Quat2, float Slerp)\n"
		"{\n"
			"// Get cosine of angle between quats.\n"
			"float RawCosom = \n"
				"Quat1.x * Quat2.x +\n"
				"Quat1.y * Quat2.y +\n"
				"Quat1.z * Quat2.z +\n"
				"Quat1.w * Quat2.w;\n"
			"// Unaligned quats - compensate, results in taking shorter route.\n"
			"float Cosom = RawCosom >= 0.f ? RawCosom : -RawCosom;\n"

			"float Scale0, Scale1;\n"

			"if( Cosom < 0.9999f )\n"
			"{	\n"
				"const float Omega = acos(Cosom);\n"
				"const float InvSin = 1.f/sin(Omega);\n"
				"Scale0 = sin( (1.f - Slerp) * Omega ) * InvSin;\n"
				"Scale1 = sin( Slerp * Omega ) * InvSin;\n"
			"}\n"
			"else\n"
			"{\n"
				"// Use linear interpolation.\n"
				"Scale0 = 1.0f - Slerp;\n"
				"Scale1 = Slerp;	\n"
			"}\n"

			"// In keeping with our flipped Cosom:\n"
			"Scale1 = RawCosom >= 0.f ? Scale1 : -Scale1;\n"

			"float4 Result;\n"
				
			"Result.x = Scale0 * Quat1.x + Scale1 * Quat2.x;\n"
			"Result.y = Scale0 * Quat1.y + Scale1 * Quat2.y;\n"
			"Result.z = Scale0 * Quat1.z + Scale1 * Quat2.z;\n"
			"Result.w = Scale0 * Quat1.w + Scale1 * Quat2.w;\n"

			"return normalize(Result);\n"
		"}\n"
	);
}
#endif

#else  //ENGINE_MINOR_VERSION < 1
#if WITH_EDITORONLY_DATA
void UNiagaraDataInterfaceHoudini::GetCommonHLSL(FString& OutHLSL)
{
	OutHLSL += TEXT("float4 q_slerp(in float4 Quat1, in float4 Quat2, float Slerp)\n"
		"{\n"
		"// Get cosine of angle between quats.\n"
		"float RawCosom = \n"
		"Quat1.x * Quat2.x +\n"
		"Quat1.y * Quat2.y +\n"
		"Quat1.z * Quat2.z +\n"
		"Quat1.w * Quat2.w;\n"
		"// Unaligned quats - compensate, results in taking shorter route.\n"
		"float Cosom = RawCosom >= 0.f ? RawCosom : -RawCosom;\n"

		"float Scale0, Scale1;\n"

		"if( Cosom < 0.9999f )\n"
		"{	\n"
		"const float Omega = acos(Cosom);\n"
		"const float InvSin = 1.f/sin(Omega);\n"
		"Scale0 = sin( (1.f - Slerp) * Omega ) * InvSin;\n"
		"Scale1 = sin( Slerp * Omega ) * InvSin;\n"
		"}\n"
		"else\n"
		"{\n"
		"// Use linear interpolation.\n"
		"Scale0 = 1.0f - Slerp;\n"
		"Scale1 = Slerp;	\n"
		"}\n"

		"// In keeping with our flipped Cosom:\n"
		"Scale1 = RawCosom >= 0.f ? Scale1 : -Scale1;\n"

		"float4 Result;\n"

		"Result.x = Scale0 * Quat1.x + Scale1 * Quat2.x;\n"
		"Result.y = Scale0 * Quat1.y + Scale1 * Quat2.y;\n"
		"Result.z = Scale0 * Quat1.z + Scale1 * Quat2.z;\n"
		"Result.w = Scale0 * Quat1.w + Scale1 * Quat2.w;\n"

		"return normalize(Result);\n"
		"}\n"
	);
}
#endif

void UNiagaraDataInterfaceHoudini::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
	ShaderParametersBuilder.AddNestedStruct<FShaderParameters>();
}

void UNiagaraDataInterfaceHoudini::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
	FNiagaraDataInterfaceProxyHoudini& DIProxy = Context.GetProxy<FNiagaraDataInterfaceProxyHoudini>();
	FShaderParameters* ShaderParameters = Context.GetParameterNestedStruct<FShaderParameters>();

	if (FHoudiniPointCacheResource* Resource = DIProxy.Resource)
	{
		ShaderParameters->NumberOfSamples = Resource->NumSamples;
		ShaderParameters->NumberOfAttributes = Resource->NumAttributes;
		ShaderParameters->NumberOfPoints = Resource->NumPoints;
		ShaderParameters->MaxNumberOfIndexesPerPoint = Resource->MaxNumberOfIndexesPerPoint;
		ShaderParameters->LastSpawnedPointId = -1;
		ShaderParameters->LastSpawnTime = -FLT_MAX;
		ShaderParameters->LastSpawnTimeRequest = -FLT_MAX;
		ShaderParameters->FloatValuesBuffer = Resource->FloatValuesGPUBuffer.SRV;
		ShaderParameters->SpecialAttributeIndexesBuffer = Resource->SpecialAttributeIndexesGPUBuffer.SRV;
		ShaderParameters->SpawnTimesBuffer = Resource->SpawnTimesGPUBuffer.SRV;
		ShaderParameters->LifeValuesBuffer = Resource->LifeValuesGPUBuffer.SRV;
		ShaderParameters->PointTypesBuffer = Resource->PointTypesGPUBuffer.SRV;
		ShaderParameters->PointValueIndexesBuffer = Resource->PointValueIndexesGPUBuffer.SRV;

		// Build the the function index to attribute index lookup table if it has not yet been built for this DI proxy
		const FNiagaraDataInterfaceParametersCS_Houdini& ShaderStorage = Context.GetShaderStorage<FNiagaraDataInterfaceParametersCS_Houdini>();
		DIProxy.UpdateFunctionIndexToAttributeIndexBuffer(ShaderStorage.FunctionIndexToAttribute);
		if (DIProxy.FunctionIndexToAttributeIndexGPUBuffer.NumBytes > 0)
		{
			ShaderParameters->FunctionIndexToAttributeIndexBuffer = DIProxy.FunctionIndexToAttributeIndexGPUBuffer.SRV;
		}
		else
		{
			ShaderParameters->FunctionIndexToAttributeIndexBuffer = FNiagaraRenderer::GetDummyIntBuffer();
		}
	}
	else
	{
		ShaderParameters->NumberOfSamples = 0;
		ShaderParameters->NumberOfAttributes = 0;
		ShaderParameters->NumberOfPoints = 0;
		ShaderParameters->MaxNumberOfIndexesPerPoint = 0;
		ShaderParameters->LastSpawnedPointId = -1;
		ShaderParameters->LastSpawnTime = -FLT_MAX;
		ShaderParameters->LastSpawnTimeRequest = -FLT_MAX;
		ShaderParameters->FloatValuesBuffer = FNiagaraRenderer::GetDummyFloatBuffer();
		ShaderParameters->SpecialAttributeIndexesBuffer = FNiagaraRenderer::GetDummyIntBuffer();
		ShaderParameters->SpawnTimesBuffer = FNiagaraRenderer::GetDummyFloatBuffer();
		ShaderParameters->LifeValuesBuffer = FNiagaraRenderer::GetDummyFloatBuffer();
		ShaderParameters->PointTypesBuffer = FNiagaraRenderer::GetDummyIntBuffer();
		ShaderParameters->PointValueIndexesBuffer = FNiagaraRenderer::GetDummyIntBuffer();
		ShaderParameters->FunctionIndexToAttributeIndexBuffer = FNiagaraRenderer::GetDummyIntBuffer();
	}
}

FNiagaraDataInterfaceParametersCS* UNiagaraDataInterfaceHoudini::CreateShaderStorage(const FNiagaraDataInterfaceGPUParamInfo& ParameterInfo, const FShaderParameterMap& ParameterMap) const
{
	FNiagaraDataInterfaceParametersCS_Houdini* ShaderStorage = new FNiagaraDataInterfaceParametersCS_Houdini();

	// Build an array of function index -> attribute name (for those functions with the Attribute specifier). If a function does not have the 
	// Attribute specifier, set to NAME_None
	const uint32 NumGeneratedFunctions = ParameterInfo.GeneratedFunctions.Num();
	ShaderStorage->FunctionIndexToAttribute.Empty(NumGeneratedFunctions);
	const FName NAME_Attribute("Attribute");
	for (const FNiagaraDataInterfaceGeneratedFunction& GeneratedFunction : ParameterInfo.GeneratedFunctions)
	{
		const FName* Attribute = GeneratedFunction.FindSpecifierValue(NAME_Attribute);
		if (Attribute != nullptr)
		{
			ShaderStorage->FunctionIndexToAttribute.Add(*Attribute);
		}
	}

	ShaderStorage->FunctionIndexToAttribute.Shrink();

	return ShaderStorage;
}

const FTypeLayoutDesc* UNiagaraDataInterfaceHoudini::GetShaderStorageType() const
{
	return &StaticGetTypeLayoutDesc<FNiagaraDataInterfaceParametersCS_Houdini>();
}

#endif  //else ENGINE_MINOR_VERSION < 1


#if WITH_EDITORONLY_DATA
	bool UNiagaraDataInterfaceHoudini::GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo & ParamInfo, const FNiagaraDataInterfaceGeneratedFunction & FunctionInfo, int FunctionInstanceIndex, FString & OutHLSL)
	{
#if ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION < 1
		// Build the buffer/variable names declared for this DI
		FString NumberOfSamplesVar = NumberOfSamplesBaseName + ParamInfo.DataInterfaceHLSLSymbol;
		FString NumberOfAttributesVar = NumberOfAttributesBaseName + ParamInfo.DataInterfaceHLSLSymbol;
		FString NumberOfPointsVar = NumberOfPointsBaseName + ParamInfo.DataInterfaceHLSLSymbol;
		FString FloatBufferVar = FloatValuesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
		FString AttributeIndexesBuffer = SpecialAttributeIndexesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
		FString SpawnTimeBuffer = SpawnTimesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
		FString LifeValuesBuffer = LifeValuesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
		FString PointTypesBuffer = PointTypesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
		FString MaxNumberOfIndexesPerPointVar = MaxNumberOfIndexesPerPointBaseName + ParamInfo.DataInterfaceHLSLSymbol;
		FString PointValueIndexesBuffer = PointValueIndexesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
		FString FunctionIndexToAttributeIndexBuffer = FunctionIndexToAttributeIndexBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
#else
		FString NumberOfSamplesVar = ParamInfo.DataInterfaceHLSLSymbol + NumberOfSamplesBaseName;
		FString NumberOfAttributesVar = ParamInfo.DataInterfaceHLSLSymbol + NumberOfAttributesBaseName;
		FString NumberOfPointsVar = ParamInfo.DataInterfaceHLSLSymbol + NumberOfPointsBaseName;
		FString FloatBufferVar = ParamInfo.DataInterfaceHLSLSymbol + FloatValuesBufferBaseName;
		FString AttributeIndexesBuffer = ParamInfo.DataInterfaceHLSLSymbol + SpecialAttributeIndexesBufferBaseName;
		FString SpawnTimeBuffer = ParamInfo.DataInterfaceHLSLSymbol + SpawnTimesBufferBaseName;
		FString LifeValuesBuffer = ParamInfo.DataInterfaceHLSLSymbol + LifeValuesBufferBaseName;
		FString PointTypesBuffer = ParamInfo.DataInterfaceHLSLSymbol + PointTypesBufferBaseName;
		FString MaxNumberOfIndexesPerPointVar = ParamInfo.DataInterfaceHLSLSymbol + MaxNumberOfIndexesPerPointBaseName;
		FString PointValueIndexesBuffer = ParamInfo.DataInterfaceHLSLSymbol + PointValueIndexesBufferBaseName;
		FString FunctionIndexToAttributeIndexBuffer = ParamInfo.DataInterfaceHLSLSymbol + FunctionIndexToAttributeIndexBufferBaseName;
#endif


// Build the shader function HLSL Code.

	// Lambda returning the HLSL code used for reading a Float value in the FloatBuffer
	auto ReadFloatInBuffer = [&](const FString& OutFloatValue, const FString& FloatSampleIndex, const FString& FloatAttrIndex)
	{
		// \t OutValue = FloatBufferName[ (SampleIndex) + ( (AttrIndex) * (NumberOfSamplesName) ) ];\n
		return TEXT("\t ") + OutFloatValue + TEXT(" = ") + FloatBufferVar + TEXT("[ (") + FloatSampleIndex + TEXT(") + ( (") + FloatAttrIndex + TEXT(") * (") + NumberOfSamplesVar + TEXT(") ) ];\n");
	};

	// Lambda returning the HLSL code for reading a Vector value in the FloatBuffer
	// It expects the In_DoSwap and In_DoScale bools to be defined before being called!
	auto ReadVectorInBuffer = [&](const FString& OutVectorValue, const FString& VectorSampleIndex, const FString& VectorAttributeIndex)
	{
		FString OutHLSLCode;
		OutHLSLCode += TEXT("\t// ReadVectorInBuffer\n");
		OutHLSLCode += TEXT("\t{\n");
			OutHLSLCode += TEXT("\t\tfloat3 temp_Value = float3(0.0, 0.0, 0.0);\n");
			OutHLSLCode += ReadFloatInBuffer(TEXT("temp_Value.x"), VectorSampleIndex, VectorAttributeIndex);
			OutHLSLCode += ReadFloatInBuffer(TEXT("temp_Value.y"), VectorSampleIndex, VectorAttributeIndex + TEXT(" + 1"));
			OutHLSLCode += ReadFloatInBuffer(TEXT("temp_Value.z"), VectorSampleIndex, VectorAttributeIndex + TEXT(" + 2"));
			OutHLSLCode += TEXT("\t\t") + OutVectorValue + TEXT(" = temp_Value;\n");
			OutHLSLCode += TEXT("\t\tif ( In_DoSwap )\n");
			OutHLSLCode += TEXT("\t\t{\n");
				OutHLSLCode += TEXT("\t\t\t") + OutVectorValue + TEXT(".y= temp_Value.z;\n");
				OutHLSLCode += TEXT("\t\t\t") + OutVectorValue + TEXT(".z= temp_Value.y;\n");
			OutHLSLCode += TEXT("\t\t}\n");
			OutHLSLCode += TEXT("\t\tif ( In_DoScale )\n");
			OutHLSLCode += TEXT("\t\t{\n");
				OutHLSLCode += TEXT("\t\t\t") + OutVectorValue + TEXT(" *= 100.0f;\n");
			OutHLSLCode += TEXT("\t\t}\n");
		OutHLSLCode += TEXT("\t}\n");
		return OutHLSLCode;
	};

	// Lambda returning the HLSL code for reading a Vector value in the FloatBuffer
	// It expects the In_DoHoudiniToUnrealConversion bool to be defined before being called!
	auto ReadVector4InBuffer = [&](const FString& OutVectorValue, const FString& VectorSampleIndex, const FString& VectorAttributeIndex)
	{
		FString OutHLSLCode;
		OutHLSLCode += TEXT("\t// ReadVector4InBuffer\n");
		OutHLSLCode += TEXT("\t{\n");
			OutHLSLCode += TEXT("\t\tfloat4 temp_Value = float4(0.0, 0.0, 0.0, 0.0);\n");
			OutHLSLCode += ReadFloatInBuffer(TEXT("temp_Value.x"), VectorSampleIndex, VectorAttributeIndex);
			OutHLSLCode += ReadFloatInBuffer(TEXT("temp_Value.y"), VectorSampleIndex, VectorAttributeIndex + TEXT(" + 1"));
			OutHLSLCode += ReadFloatInBuffer(TEXT("temp_Value.z"), VectorSampleIndex, VectorAttributeIndex + TEXT(" + 2"));
			OutHLSLCode += ReadFloatInBuffer(TEXT("temp_Value.w"), VectorSampleIndex, VectorAttributeIndex + TEXT(" + 3"));
			OutHLSLCode += TEXT("\t\t") + OutVectorValue + TEXT(" = temp_Value;\n");
			OutHLSLCode += TEXT("\t\tif ( In_DoHoudiniToUnrealConversion )\n");
			OutHLSLCode += TEXT("\t\t{\n");
				OutHLSLCode += TEXT("\t\t\t") + OutVectorValue + TEXT(".x= -temp_Value.x;\n");
				OutHLSLCode += TEXT("\t\t\t") + OutVectorValue + TEXT(".y= -temp_Value.z;\n");
				OutHLSLCode += TEXT("\t\t\t") + OutVectorValue + TEXT(".z= -temp_Value.y;\n");
			OutHLSLCode += TEXT("\t\t}\n");
		OutHLSLCode += TEXT("\t}\n");
		return OutHLSLCode;
	};

	// Lambda returning the HLSL code for reading a special attribute's index in the buffer
	auto GetSpecAttributeIndex = [&](const EHoudiniAttributes& Attr)
	{
		FString OutHLSLCode;
		OutHLSLCode += AttributeIndexesBuffer + TEXT("[") + FString::FromInt(Attr) + TEXT("]");
		return OutHLSLCode;
	};

	// Lambda returning an HLSL expression for testing if two floats are nearly equal
	auto IsNearlyEqualExpression = [&](const FString& In_A, const FString& In_B, const FString& In_ErrorTolerance = "1.e-8f")
	{
		return FString::Printf(TEXT("( abs( %s - %s ) <=  %s )"), *In_A, *In_B, *In_ErrorTolerance);
	};

	// Lambda returning the HLSL code for getting the previous and next sample indexes for a given particle at a given time	
	auto GetSampleIndexesForPointAtTime = [&](const FString& In_PointID, const FString& In_Time, const FString& Out_PreviousSampleIndex, const FString& Out_NextSampleIndex, const FString& Out_Weight)
	{
		FString OutHLSLCode;
		OutHLSLCode += TEXT("\t// GetSampleIndexesForPointAtTime\n");
		OutHLSLCode += TEXT("\t{\n");

			OutHLSLCode += TEXT("\t\tbool prev_time_valid = false;\n");
			OutHLSLCode += TEXT("\t\tfloat prev_time = -1.0f;\n");
			OutHLSLCode += TEXT("\t\tbool next_time_valid = false;\n");
			OutHLSLCode += TEXT("\t\tfloat next_time = -1.0f;\n");
			OutHLSLCode += TEXT("\t\tbool is_weight_set = false;\n");

			// Look at all the values for this Point
			OutHLSLCode += TEXT("\t\tfor( int n = 0; n < ") + MaxNumberOfIndexesPerPointVar + TEXT("; n++ )\n\t\t{\n");
				OutHLSLCode += TEXT("\t\t\tint current_sample_index = ") + PointValueIndexesBuffer + TEXT("[ (") + In_PointID + TEXT(") * ") + MaxNumberOfIndexesPerPointVar + TEXT(" + n ];\n");
				OutHLSLCode += TEXT("\t\t\tif ( current_sample_index < 0 ){ break; }\n");

				OutHLSLCode += TEXT("\t\t\tfloat current_time = -1.0f;\n");
				OutHLSLCode += TEXT("\t\t\tint time_attr_index = ") + GetSpecAttributeIndex(EHoudiniAttributes::TIME) + TEXT(";\n");
				OutHLSLCode += TEXT("\t\t\tif ( time_attr_index < 0 || time_attr_index >= ") + NumberOfAttributesVar + TEXT(" )\n");
					OutHLSLCode += TEXT("\t\t\t\t{ current_time = 0.0f; }\n");
				OutHLSLCode += TEXT("\t\t\telse\n");
					OutHLSLCode += TEXT("\t\t\t{") + ReadFloatInBuffer(TEXT("current_time"), TEXT("current_sample_index"), TEXT("time_attr_index")) + TEXT(" }\n");

				OutHLSLCode += TEXT("\t\t\tif ( ") + IsNearlyEqualExpression("current_time", In_Time) + TEXT(" )\n");
					OutHLSLCode += TEXT("\t\t\t\t{ ") + Out_PreviousSampleIndex + TEXT(" = current_sample_index; ") + Out_NextSampleIndex + TEXT(" = current_sample_index; ") + Out_Weight + TEXT(" = 1.0; is_weight_set = true; break;}\n");
				OutHLSLCode += TEXT("\t\t\telse if ( current_time < (") + In_Time + TEXT(") ){\n");
					OutHLSLCode += TEXT("\t\t\t\tif ( !prev_time_valid || prev_time < current_time )\n");
						OutHLSLCode += TEXT("\t\t\t\t\t { ") + Out_PreviousSampleIndex + TEXT(" = current_sample_index; prev_time = current_time; prev_time_valid = true; }\n");
				OutHLSLCode += TEXT("\t\t\t}\n");
				OutHLSLCode += TEXT("\t\t\telse if ( !next_time_valid || next_time > current_time )\n");
					OutHLSLCode += TEXT("\t\t\t\t { ") + Out_NextSampleIndex + TEXT(" = current_sample_index; next_time = current_time; next_time_valid = true; break; }\n");
			OutHLSLCode += TEXT("\t\t}\n");

			// Calculate the weight. We can only calculate the weight if at least one of Previous or Next Sample Index is valid,
			// or both prev_time_valid is true and next_time_valid is true. The other case is where we found a sample that
			// matches In_Time: that is handled in the for loop above, and both sample indices and weight are already set.
			OutHLSLCode += TEXT("\t\tif ( ") + Out_PreviousSampleIndex + TEXT(" >= 0 || ") + Out_NextSampleIndex + TEXT(" >= 0 )\n");
				OutHLSLCode += TEXT("\t\t{\n\t\t\tif ( ") + Out_PreviousSampleIndex + TEXT(" < 0 )\n");
					OutHLSLCode += TEXT("\t\t\t\t{ ") + Out_Weight + TEXT(" = 0.0f; ") + Out_PreviousSampleIndex + TEXT(" = ") + Out_NextSampleIndex + TEXT("; is_weight_set = true;}\n");
				OutHLSLCode += TEXT("\t\t\telse if ( ") + Out_NextSampleIndex + TEXT(" < 0 )\n");
					OutHLSLCode += TEXT("\t\t\t\t{ ") + Out_Weight + TEXT(" = 1.0f; ") + Out_NextSampleIndex + TEXT(" = ") + Out_PreviousSampleIndex + TEXT("; is_weight_set = true;}\n");
			OutHLSLCode += TEXT("\t\t}\n");

			OutHLSLCode += TEXT("\t\tif ( !is_weight_set && prev_time_valid && next_time_valid )\n");
				OutHLSLCode += TEXT("\t\t\t{ ") + Out_Weight + TEXT(" = ( ( (") + In_Time + TEXT(") - prev_time ) / ( next_time - prev_time ) ); }\n");

		OutHLSLCode += TEXT("\t}\n");
		return OutHLSLCode;
	};

	// Build each function's HLSL code
	if (FunctionInfo.DefinitionName == GetFloatValueName)
	{
		// GetFloatValue(int In_SampleIndex, int In_AttributeIndex, out float Out_Value)		
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_SampleIndex, int In_AttributeIndex, out float Out_Value) \n{\n");

			OutHLSL += ReadFloatInBuffer(TEXT("Out_Value"), TEXT("In_SampleIndex"), TEXT("In_AttributeIndex"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetVectorValueName)
	{
		// GetVectorValue(int In_SampleIndex, int In_AttributeIndex, out float3 Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_SampleIndex, int In_AttributeIndex, out float3 Out_Value) \n{\n");

			OutHLSL += TEXT("\tbool In_DoSwap = true;\n");
			OutHLSL += TEXT("\tbool In_DoScale = true;\n");
			OutHLSL += ReadVectorInBuffer(TEXT("Out_Value"), TEXT("In_SampleIndex"), TEXT("In_AttributeIndex"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetVectorValueExName)
	{
		// GetVectorValueEx(int In_SampleIndex, int In_AttributeIndex, bool In_DoSwap, bool In_DoScale, out float3 Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_SampleIndex, int In_AttributeIndex, bool In_DoSwap, bool In_DoScale, out float3 Out_Value) \n{\n");
		
			OutHLSL += ReadVectorInBuffer(TEXT("Out_Value"), TEXT("In_SampleIndex"), TEXT("In_AttributeIndex"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetVector4ValueName)
	{
		// GetVector4Value(int In_SampleIndex, int In_AttributeIndex, out float4 Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_SampleIndex, int In_AttributeIndex, out float4 Out_Value) \n{\n");

			OutHLSL += TEXT("\tbool In_DoHoudiniToUnrealConversion = false;\n");			
			OutHLSL += ReadVector4InBuffer(TEXT("Out_Value"), TEXT("In_SampleIndex"), TEXT("In_AttributeIndex"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetQuatValueName)
	{
		// GetQuatValue(int In_SampleIndex, int In_AttributeIndex, bool In_DoHoudiniToUnrealConversion, out float4 Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_SampleIndex, int In_AttributeIndex, bool In_DoHoudiniToUnrealConversion, out float4 Out_Value) \n{\n");
		
			OutHLSL += ReadVector4InBuffer(TEXT("Out_Value"), TEXT("In_SampleIndex"), TEXT("In_AttributeIndex"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetPositionName)
	{
		// GetPosition(int In_SampleIndex, out float3 Out_Position)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_SampleIndex, out float3 Out_Position) \n{\n");

			OutHLSL += TEXT("\tbool In_DoSwap = true;\n");
			OutHLSL += TEXT("\tbool In_DoScale = true;\n");
			OutHLSL += TEXT("\tint In_AttributeIndex = ") + GetSpecAttributeIndex( EHoudiniAttributes::POSITION ) + TEXT(";\n");
			OutHLSL += ReadVectorInBuffer(TEXT("Out_Position"), TEXT("In_SampleIndex"), TEXT("In_AttributeIndex"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetPositionAndTimeName)
	{
		// GetPositionAndTime(int In_SampleIndex, out float3 Out_Position, out float Out_Time)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_SampleIndex, out float3 Out_Position, out float Out_Time) \n{\n");

			OutHLSL += TEXT("\tbool In_DoSwap = true;\n");
			OutHLSL += TEXT("\tbool In_DoScale = true;\n");
			OutHLSL += TEXT("\tint In_PosAttrIndex = ") + GetSpecAttributeIndex(EHoudiniAttributes::POSITION) + TEXT(";\n");
			OutHLSL += ReadVectorInBuffer(TEXT("Out_Position"), TEXT("In_SampleIndex"), TEXT("In_PosAttrIndex"));

			OutHLSL += TEXT("\tint In_TimeAttributeIndex = ") + GetSpecAttributeIndex(EHoudiniAttributes::TIME) + TEXT(";\n");
			OutHLSL += ReadFloatInBuffer(TEXT("Out_Time"), TEXT("In_SampleIndex"), TEXT("In_TimeAttributeIndex"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetNormalName)
	{
		// GetNormal(int In_SampleIndex, out float3 Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_SampleIndex, out float3 Out_Normal) \n{\n");

			OutHLSL += TEXT("\tbool In_DoSwap = true;\n");
			OutHLSL += TEXT("\tbool In_DoScale = false;\n");
			OutHLSL += TEXT("\tint In_AttributeIndex = ") + GetSpecAttributeIndex(EHoudiniAttributes::NORMAL) + TEXT(";\n");
			OutHLSL += ReadVectorInBuffer(TEXT("Out_Normal"), TEXT("In_SampleIndex"), TEXT("In_AttributeIndex"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetTimeName)
	{
		// GetTime(int In_SampleIndex, out float Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_SampleIndex, out float Out_Time) \n{\n");

			OutHLSL += TEXT("\tint In_TimeAttributeIndex = ") + GetSpecAttributeIndex(EHoudiniAttributes::TIME) + TEXT(";\n");
			OutHLSL += ReadFloatInBuffer(TEXT("Out_Time"), TEXT("In_SampleIndex"), TEXT("In_TimeAttributeIndex"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetVelocityName)
	{
		// GetVelocity(int In_SampleIndex, out float3 Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_SampleIndex, out float3 Out_Velocity) \n{\n");

			OutHLSL += TEXT("\tbool In_DoSwap = true;\n");
			OutHLSL += TEXT("\tbool In_DoScale = false;\n");
			OutHLSL += TEXT("\tint In_AttributeIndex = ") + GetSpecAttributeIndex(EHoudiniAttributes::VELOCITY) + TEXT(";\n");
			OutHLSL += ReadVectorInBuffer(TEXT("Out_Velocity"), TEXT("In_SampleIndex"), TEXT("In_AttributeIndex"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetImpulseName)
	{
		// GetImpulse(int In_SampleIndex, out float Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_SampleIndex, out float Out_Impulse) \n{\n");

		OutHLSL += TEXT("\tint In_ImpulseAttrIndex = ") + GetSpecAttributeIndex(EHoudiniAttributes::IMPULSE) + TEXT(";\n");
		OutHLSL += ReadFloatInBuffer(TEXT("Out_Impulse"), TEXT("In_SampleIndex"), TEXT("In_ImpulseAttrIndex"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetColorName)
	{
		// GetColor(int In_SampleIndex, out float4 Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_SampleIndex, out float4 Out_Color) \n{\n");

			OutHLSL += TEXT("\tfloat3 temp_color = float3(1.0, 1.0, 1.0);\n");
			OutHLSL += TEXT("\tint In_ColorAttrIndex = ") + GetSpecAttributeIndex(EHoudiniAttributes::COLOR) + TEXT(";\n");
			OutHLSL += TEXT("\tbool In_DoSwap = false;\n");
			OutHLSL += TEXT("\tbool In_DoScale = false;\n");
			OutHLSL += ReadVectorInBuffer(TEXT("temp_color"), TEXT("In_SampleIndex"), TEXT("In_ColorAttrIndex"));

			OutHLSL += TEXT("\tfloat temp_alpha = 1.0;\n");
			OutHLSL += TEXT("\tint In_TimeAttributeIndex = ") + GetSpecAttributeIndex(EHoudiniAttributes::TIME) + TEXT(";\n");
			OutHLSL += ReadFloatInBuffer(TEXT("temp_alpha"), TEXT("In_SampleIndex"), TEXT("In_TimeAttributeIndex"));

			OutHLSL += TEXT("Out_Color.x = temp_color.x;\n");
			OutHLSL += TEXT("Out_Color.y = temp_color.y;\n");
			OutHLSL += TEXT("Out_Color.z = temp_color.z;\n");
			OutHLSL += TEXT("Out_Color.w = temp_alpha;\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetNumberOfPointsName)
	{
		// GetNumberOfPoints(out int Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(out int Out_NumPoints) \n{\n");
			OutHLSL += TEXT("\tOut_NumPoints = ") + NumberOfPointsVar + TEXT(";\n");
		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetNumberOfSamplesName)
	{
		// GetNumberOfSamples(out int Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(out int Out_NumSamples) \n{\n");
			OutHLSL += TEXT("\tOut_NumSamples = ") + NumberOfSamplesVar + TEXT(";\n");
		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetNumberOfAttributesName)
	{
		// GetNumberOfSamples(out int Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(out int Out_NumAttributes) \n{\n");
			OutHLSL += TEXT("\tOut_NumAttributes = ") + NumberOfAttributesVar + TEXT(";\n");
		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetLastSampleIndexAtTimeName)
	{
		// GetLastSampleIndexAtTime(float In_Time, out int Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(float In_Time, out int Out_Value) \n{\n");

			OutHLSL += TEXT("\tint In_TimeAttributeIndex = ") + GetSpecAttributeIndex( EHoudiniAttributes::TIME ) + TEXT(";\n");
			OutHLSL += TEXT("\tfloat temp_time = 1.0;\n");
			OutHLSL += ReadFloatInBuffer(TEXT("temp_time"), NumberOfSamplesVar + TEXT("- 1"), TEXT("In_TimeAttributeIndex"));
			OutHLSL += TEXT("\tif ( temp_time < In_Time ) { Out_Value = ") + NumberOfSamplesVar + TEXT(" - 1; return; }\n");
			OutHLSL += TEXT("\tint lastSampleIndex = -1;\n");
			OutHLSL += TEXT("\tfor( int n = 0; n < ") + NumberOfSamplesVar + TEXT("; n++ )\n\t{\n");
				OutHLSL += TEXT("\t") + ReadFloatInBuffer(TEXT("temp_time"), TEXT("n"), TEXT("In_TimeAttributeIndex"));
				OutHLSL += TEXT("\t\tif ( temp_time == In_Time ){ lastSampleIndex = n ;}");
				OutHLSL += TEXT("\t\telse if ( temp_time > In_Time ){ lastSampleIndex = n -1; return;}");
				OutHLSL += TEXT("\t\tif ( lastSampleIndex == -1 ){ lastSampleIndex = ") + NumberOfSamplesVar + TEXT(" - 1; }");
			OutHLSL += TEXT("\t}\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetPointIDsToSpawnAtTimeName)
	{
		// GetPointIDsToSpawnAtTime(float In_Time, float In_LastSpawnTime, float In_LastSpawnTimeRequest, int In_LastSpawnPointID, out int Out_MinID, out int Out_MaxID, out int Out_Count, out float Out_LastSpawnTime, out float Out_LastSpawnTimeRequest, out int Out_LastSpawnPointID)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(float In_Time, float In_LastSpawnTime, float In_LastSpawnTimeRequest, int In_LastSpawnedPointID, out int Out_MinID, out int Out_MaxID, out int Out_Count, out float Out_LastSpawnTime, out float Out_LastSpawnTimeRequest, out int Out_LastSpawnPointID) \n{\n");
			
			OutHLSL += TEXT("\tint time_attr_index = ") + GetSpecAttributeIndex(EHoudiniAttributes::TIME) + TEXT(";\n");
			OutHLSL += TEXT("\tif( time_attr_index < 0 )\n");
				OutHLSL += TEXT("\t\t{ Out_Count = ") + NumberOfPointsVar +TEXT("; Out_MinID = 0; Out_MaxID = Out_Count - 1; In_LastSpawnTimeRequest = In_Time; return; }\n");

			// GetLastPointIDToSpawnAtTime
			OutHLSL += TEXT("\tint last_id = -1;\n");
			OutHLSL += TEXT("\tif ( ") + SpawnTimeBuffer + TEXT("[ ") + NumberOfPointsVar + TEXT(" - 1 ] < In_Time && In_Time > In_LastSpawnTimeRequest )\n");
			OutHLSL += TEXT("\t{\n");
				OutHLSL += TEXT("\t\tlast_id = ") + NumberOfPointsVar + TEXT(" - 1;\n");
			OutHLSL += TEXT("\t}\n");
			OutHLSL += TEXT("\telse\n");
			OutHLSL += TEXT("\t{\n");
				OutHLSL += TEXT("\t\tfloat temp_time = 0;\n");
				OutHLSL += TEXT("\t\tfor( int n = 0; n < ") + NumberOfPointsVar + TEXT("; n++ )\n\t\t{\n");
					OutHLSL += TEXT("\t\t\ttemp_time = ") + SpawnTimeBuffer + TEXT("[ n ];\n");
					OutHLSL += TEXT("\t\t\tif ( temp_time > In_Time )\n");
						OutHLSL += TEXT("\t\t\t\t{ break; }\n");
					OutHLSL += TEXT("\t\t\tlast_id = n;\n");
				OutHLSL += TEXT("\t\t}\n");
			OutHLSL += TEXT("\t}\n");

			// First, detect if we need to reset LastSpawnedPointID (after a loop of the emitter)
			OutHLSL += TEXT("\tif ( last_id < In_LastSpawnedPointID || In_Time <= In_LastSpawnTime || In_Time <= In_LastSpawnTimeRequest )\n");
				OutHLSL += TEXT("\t\t{ In_LastSpawnedPointID = -1; }\n");

			OutHLSL += TEXT("\tif ( last_id < 0 )\n");
			OutHLSL += TEXT("\t{\n");
				// Nothing to spawn, t is lower than the point's time
				OutHLSL += TEXT("\t\t In_LastSpawnedPointID = -1;\n");
				OutHLSL += TEXT("\t\tOut_MinID = last_id;\n");
				OutHLSL += TEXT("\t\tOut_MaxID = last_id;\n");
				OutHLSL += TEXT("\t\tOut_Count = 0;\n");
			OutHLSL += TEXT("\t}\n");
			OutHLSL += TEXT("\telse\n");
			OutHLSL += TEXT("\t{\n");

				// The last time value in the CSV is lower than t, spawn everything if we didnt already!
				OutHLSL += TEXT("\t\tif ( last_id >= In_LastSpawnedPointID )\n");
				OutHLSL += TEXT("\t\t{\n");
					OutHLSL += TEXT("\t\t\tlast_id = last_id - 1;\n");
				OutHLSL += TEXT("\t\t}\n");

				OutHLSL += TEXT("\t\tif ( last_id == In_LastSpawnedPointID )\n");
				OutHLSL += TEXT("\t\t{\n");
					// We dont have any new point to spawn
					OutHLSL += TEXT("\t\t\tOut_MinID = last_id;\n");
					OutHLSL += TEXT("\t\t\tOut_MaxID = last_id;\n");
					OutHLSL += TEXT("\t\t\tOut_Count = 0;\n");
				OutHLSL += TEXT("\t\t}\n");
				OutHLSL += TEXT("\t\telse\n");
				OutHLSL += TEXT("\t\t{\n");
					// We have points to spawn at time t
					OutHLSL += TEXT("\t\t\tOut_MinID = In_LastSpawnedPointID + 1;\n");
					OutHLSL += TEXT("\t\t\tOut_MaxID = last_id;\n");
					OutHLSL += TEXT("\t\t\tOut_Count = Out_MaxID - Out_MinID + 1;\n");

					OutHLSL += TEXT("\t\t\tIn_LastSpawnedPointID = Out_MaxID;\n");
					OutHLSL += TEXT("\t\t\tIn_LastSpawnTime = In_Time;\n");
				OutHLSL += TEXT("\t\t}\n");

			OutHLSL += TEXT("\t}\n");

			OutHLSL += TEXT("\tIn_LastSpawnTimeRequest = In_Time;\n");

			// Output the sim state variables
			OutHLSL += TEXT("\tOut_LastSpawnTimeRequest = In_LastSpawnTimeRequest;\n");
			OutHLSL += TEXT("\tOut_LastSpawnTime = In_LastSpawnTime;\n");
			OutHLSL += TEXT("\tIn_LastSpawnedPointID = In_LastSpawnedPointID;\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetSampleIndexesForPointAtTimeName)
	{
		// GetSampleIndexesForPointAtTime(int In_PointID, float In_Time, out int Out_PreviousSampleIndex, out int Out_NextSampleIndex, out float Out_Weight)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_PointID, float In_Time, out int Out_PreviousSampleIndex, out int Out_NextSampleIndex, out float Out_Weight) \n{\n");

			OutHLSL += TEXT("\tOut_PreviousSampleIndex = 0;\n\tOut_NextSampleIndex = 0;\n\tOut_Weight = 0;\n");
			OutHLSL += GetSampleIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("Out_PreviousSampleIndex"), TEXT("Out_NextSampleIndex"), TEXT("Out_Weight"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetPointPositionAtTimeName)
	{
		// GetPointPositionAtTime(int In_PointID, float In_Time, out float3 Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_PointID, float In_Time, out float3 Out_Value) \n{\n");
		OutHLSL += TEXT("Out_Value = float3( 0.0, 0.0, 0.0 );\n");
			OutHLSL += TEXT("\tint pos_attr_index = ") + GetSpecAttributeIndex(EHoudiniAttributes::POSITION) + TEXT(";\n");
			OutHLSL += TEXT("\tint prev_index = 0;int next_index = 0;float weight = 0.0f;\n");
			OutHLSL += GetSampleIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));

			OutHLSL += TEXT("\tbool In_DoSwap = true;\n");
			OutHLSL += TEXT("\tbool In_DoScale = true;\n");

			OutHLSL += TEXT("\tfloat3 prev_vector = float3( 0.0, 0.0, 0.0 );\n");
			OutHLSL += ReadVectorInBuffer(TEXT("prev_vector"), TEXT("prev_index"), TEXT("pos_attr_index"));
			OutHLSL += TEXT("\tfloat3 next_vector = float3( 0.0, 0.0, 0.0 );\n");
			OutHLSL += ReadVectorInBuffer(TEXT("next_vector"), TEXT("next_index"), TEXT("pos_attr_index"));

			OutHLSL += TEXT("\tOut_Value = lerp(prev_vector, next_vector, weight);\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetPointValueAtTimeName)
	{
		// GetPointValueAtTime(int In_PointID, int In_AttributeIndex, float In_Time, out float Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_PointID, int In_AttributeIndex, float In_Time, out float Out_Value) \n{\n");
		
		OutHLSL += TEXT("\tint prev_index = -1;int next_index = -1;float weight = 1.0f;\n");
		OutHLSL += GetSampleIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));

		OutHLSL += TEXT("\tfloat prev_value;\n");
		OutHLSL += ReadFloatInBuffer(TEXT("prev_value"), TEXT("prev_index"), TEXT("In_AttributeIndex"));
		OutHLSL += TEXT("\tfloat next_value;\n");
		OutHLSL += ReadFloatInBuffer(TEXT("next_value"), TEXT("next_index"), TEXT("In_AttributeIndex"));

		OutHLSL += TEXT("\tOut_Value = lerp(prev_value, next_value, weight);\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetPointVectorValueAtTimeName)
	{
		// GetPointVectorValueAtTime(int In_PointID, int In_AttributeIndex, float In_Time, out float3 Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_PointID, int In_AttributeIndex, float In_Time, out float3 Out_Value) \n{\n");

		OutHLSL += TEXT("\tint prev_index = -1;int next_index = -1;float weight = 1.0f;\n");
		OutHLSL += GetSampleIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));

		OutHLSL += TEXT("\tbool In_DoSwap = true;\n");
		OutHLSL += TEXT("\tbool In_DoScale = true;\n");

		OutHLSL += TEXT("\tfloat3 prev_vector;\n");
		OutHLSL += ReadVectorInBuffer(TEXT("prev_vector"), TEXT("prev_index"), TEXT("In_AttributeIndex"));
		OutHLSL += TEXT("\tfloat3 next_vector;\n");
		OutHLSL += ReadVectorInBuffer(TEXT("next_vector"), TEXT("next_index"), TEXT("In_AttributeIndex"));

		OutHLSL += TEXT("\tOut_Value = lerp(prev_vector, next_vector, weight);\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetPointVectorValueAtTimeExName)
	{
		// GetPointVectorValueAtTimeEx(int In_PointID, int In_AttributeIndex, float In_Time, bool In_DoSwap, bool In_DoScale, out float3 Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_PointID, int In_AttributeIndex, float In_Time, bool In_DoSwap, bool In_DoScale, out float3 Out_Value) \n{\n");

			OutHLSL += TEXT("\tint prev_index = -1;int next_index = -1;float weight = 1.0f;\n");
			OutHLSL += GetSampleIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));

			OutHLSL += TEXT("\tfloat3 prev_vector;\n");
			OutHLSL += ReadVectorInBuffer(TEXT("prev_vector"), TEXT("prev_index"), TEXT("In_AttributeIndex"));
			OutHLSL += TEXT("\tfloat3 next_vector;\n");
			OutHLSL += ReadVectorInBuffer(TEXT("next_vector"), TEXT("next_index"), TEXT("In_AttributeIndex"));

			OutHLSL += TEXT("\tOut_Value = lerp(prev_vector, next_vector, weight);\n");

		OutHLSL += TEXT("\n}\n");

		return true;
	}
	else if (FunctionInfo.DefinitionName == GetPointVector4ValueAtTimeName)
	{
		// GetPointVector4ValueAtTime(int In_PointID, int In_AttributeIndex, float In_Time, out float3 Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_PointID, int In_AttributeIndex, float In_Time, out float4 Out_Value) \n{\n");

		OutHLSL += TEXT("\tint prev_index = -1;int next_index = -1;float weight = 1.0f;\n");
		OutHLSL += GetSampleIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));

		OutHLSL += TEXT("\tbool In_DoHoudiniToUnrealConversion = false;\n");

		OutHLSL += TEXT("\tfloat4 prev_vector;\n");
		OutHLSL += ReadVector4InBuffer(TEXT("prev_vector"), TEXT("prev_index"), TEXT("In_AttributeIndex"));
		OutHLSL += TEXT("\tfloat4 next_vector;\n");
		OutHLSL += ReadVector4InBuffer(TEXT("next_vector"), TEXT("next_index"), TEXT("In_AttributeIndex"));

		OutHLSL += TEXT("\tOut_Value = lerp(prev_vector, next_vector, weight);\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetPointQuatValueAtTimeName)
	{
		// GetPointQuatValueAtTime(int In_PointID, int In_AttributeIndex, float In_Time, out float3 Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_PointID, int In_AttributeIndex, float In_Time, bool In_DoHoudiniToUnrealConversion, out float4 Out_Value) \n{\n");

		OutHLSL += TEXT("\tint prev_index = -1;int next_index = -1;float weight = 1.0f;\n");
		OutHLSL += GetSampleIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));

		OutHLSL += TEXT("\tfloat4 prev_quat;\n");
		OutHLSL += ReadVector4InBuffer(TEXT("prev_quat"), TEXT("prev_index"), TEXT("In_AttributeIndex"));
		OutHLSL += TEXT("\tfloat4 next_quat;\n");
		OutHLSL += ReadVector4InBuffer(TEXT("next_quat"), TEXT("next_index"), TEXT("In_AttributeIndex"));

		OutHLSL += TEXT("\tOut_Value = q_slerp(prev_quat, next_quat, weight);\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetPointLifeName)
	{
		// GetPointLife(int In_PointID, out float Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_PointID, out float Out_Value) \n{\n");

			OutHLSL += TEXT("\tif ( ( In_PointID < 0 ) || ( In_PointID >= ") + NumberOfPointsVar + TEXT(") )\n");
				OutHLSL += TEXT("\t\t{Out_Value = -1.0f; return;}\n");
			OutHLSL += TEXT("\tOut_Value = ") + LifeValuesBuffer + TEXT("[ In_PointID ];\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetPointLifeAtTimeName)
	{
		// GetPointLifeAtTime(int In_PointID, float In_Time, out float Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_PointID, float In_Time, out float Out_Value) \n{\n");

			OutHLSL += TEXT("\tif ( ( In_PointID < 0 ) || ( In_PointID >= ") + NumberOfPointsVar+ TEXT(") )\n");
				OutHLSL += TEXT("\t\t{Out_Value = -1; return;}\n");
			OutHLSL += TEXT("\telse if ( In_Time < ") + SpawnTimeBuffer + TEXT("[ In_PointID ] )\n");
			OutHLSL += TEXT("\t{\n");
				OutHLSL += TEXT("\t\tOut_Value = ") + LifeValuesBuffer + TEXT("[ In_PointID ];\n");
			OutHLSL += TEXT("\t}\n");
			OutHLSL += TEXT("\telse\n");
			OutHLSL += TEXT("\t{\n");
				OutHLSL += TEXT("\t\tOut_Value = ") + LifeValuesBuffer + TEXT("[ In_PointID ] - ( In_Time - ") + SpawnTimeBuffer + TEXT("[ In_PointID ] );\n");
			OutHLSL += TEXT("\t}\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetPointTypeName)
	{
		// GetPointType(int In_PointID, out int Out_Value)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_PointID, out int Out_Value) \n{\n");

			OutHLSL += TEXT("\tif ( ( In_PointID < 0 ) || ( In_PointID >= ") + NumberOfPointsVar+ TEXT(") )\n");
				OutHLSL += TEXT("\t\t{Out_Value = -1; return;}\n");
			OutHLSL += TEXT("\tOut_Value = ") + PointTypesBuffer + TEXT("[ In_PointID ];\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	// BEGIN: helper GetPoint<AttrName>AtTime functions, can potentially be removed once get attr by string functions are functional
	else if (FunctionInfo.DefinitionName == GetPointNormalAtTimeName)
	{
		// GetPointNormalAtTime(int In_PointID, float In_Time, out float3 Out_Normal)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_PointID, float In_Time, out float3 Out_Normal) \n{\n");

			OutHLSL += TEXT("\tint In_AttributeIndex = ") + GetSpecAttributeIndex(EHoudiniAttributes::NORMAL) + TEXT(";\n");

			OutHLSL += TEXT("\tint prev_index = -1;int next_index = -1;float weight = 1.0f;\n");
			OutHLSL += GetSampleIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));

			OutHLSL += TEXT("\tbool In_DoSwap = true;\n");
			OutHLSL += TEXT("\tbool In_DoScale = false;\n");

			OutHLSL += TEXT("\tfloat3 prev_vector;\n");
			OutHLSL += ReadVectorInBuffer(TEXT("prev_vector"), TEXT("prev_index"), TEXT("In_AttributeIndex"));
			OutHLSL += TEXT("\tfloat3 next_vector;\n");
			OutHLSL += ReadVectorInBuffer(TEXT("next_vector"), TEXT("next_index"), TEXT("In_AttributeIndex"));

			OutHLSL += TEXT("\tOut_Normal = lerp(prev_vector, next_vector, weight);\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetPointColorAtTimeName)
	{
		// GetPointColorAtTime(int In_PointID, float In_Time, out float3 Out_Color)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_PointID, float In_Time, out float3 Out_Color) \n{\n");

			OutHLSL += TEXT("\tint In_AttributeIndex = ") + GetSpecAttributeIndex(EHoudiniAttributes::COLOR) + TEXT(";\n");

			OutHLSL += TEXT("\tint prev_index = -1;int next_index = -1;float weight = 1.0f;\n");
			OutHLSL += GetSampleIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));

			OutHLSL += TEXT("\tbool In_DoSwap = true;\n");
			OutHLSL += TEXT("\tbool In_DoScale = false;\n");

			OutHLSL += TEXT("\tfloat3 prev_vector;\n");
			OutHLSL += ReadVectorInBuffer(TEXT("prev_vector"), TEXT("prev_index"), TEXT("In_AttributeIndex"));
			OutHLSL += TEXT("\tfloat3 next_vector;\n");
			OutHLSL += ReadVectorInBuffer(TEXT("next_vector"), TEXT("next_index"), TEXT("In_AttributeIndex"));

			OutHLSL += TEXT("\tOut_Color = lerp(prev_vector, next_vector, weight);\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetPointAlphaAtTimeName)
	{
		// GetPointAlphaAtTime(int In_PointID, float In_Time, out float Out_Alpha)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_PointID, float In_Time, out float Out_Alpha) \n{\n");

			OutHLSL += TEXT("\tint In_AttributeIndex = ") + GetSpecAttributeIndex(EHoudiniAttributes::ALPHA) + TEXT(";\n");

			OutHLSL += TEXT("\tint prev_index = -1;int next_index = -1;float weight = 1.0f;\n");
			OutHLSL += GetSampleIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));

			OutHLSL += TEXT("\tfloat3 prev_value;\n");
			OutHLSL += ReadFloatInBuffer(TEXT("prev_value"), TEXT("prev_index"), TEXT("In_AttributeIndex"));
			OutHLSL += TEXT("\tfloat3 next_value;\n");
			OutHLSL += ReadFloatInBuffer(TEXT("next_value"), TEXT("next_index"), TEXT("In_AttributeIndex"));

			OutHLSL += TEXT("\tOut_Alpha = lerp(prev_value, next_value, weight);\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetPointVelocityAtTimeName)
	{
		// GetPointVelocityAtTime(int In_PointID, float In_Time, out float3 Out_Velocity)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_PointID, float In_Time, out float3 Out_Velocity) \n{\n");

			OutHLSL += TEXT("\tint In_AttributeIndex = ") + GetSpecAttributeIndex(EHoudiniAttributes::VELOCITY) + TEXT(";\n");

			OutHLSL += TEXT("\tint prev_index = -1;int next_index = -1;float weight = 1.0f;\n");
			OutHLSL += GetSampleIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));

			OutHLSL += TEXT("\tbool In_DoSwap = true;\n");
			OutHLSL += TEXT("\tbool In_DoScale = true;\n");

			OutHLSL += TEXT("\tfloat3 prev_vector;\n");
			OutHLSL += ReadVectorInBuffer(TEXT("prev_vector"), TEXT("prev_index"), TEXT("In_AttributeIndex"));
			OutHLSL += TEXT("\tfloat3 next_vector;\n");
			OutHLSL += ReadVectorInBuffer(TEXT("next_vector"), TEXT("next_index"), TEXT("In_AttributeIndex"));

			OutHLSL += TEXT("\tOut_Velocity = lerp(prev_vector, next_vector, weight);\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetPointImpulseAtTimeName)
	{
		// GetPointImpulseAtTime(int In_PointID, float In_Time, out float Out_Impulse)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_PointID, float In_Time, out float Out_Impulse) \n{\n");

			OutHLSL += TEXT("\tint In_AttributeIndex = ") + GetSpecAttributeIndex(EHoudiniAttributes::ALPHA) + TEXT(";\n");

			OutHLSL += TEXT("\tint prev_index = -1;int next_index = -1;float weight = 1.0f;\n");
			OutHLSL += GetSampleIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));

			OutHLSL += TEXT("\tfloat3 prev_value;\n");
			OutHLSL += ReadFloatInBuffer(TEXT("prev_value"), TEXT("prev_index"), TEXT("In_AttributeIndex"));
			OutHLSL += TEXT("\tfloat3 next_value;\n");
			OutHLSL += ReadFloatInBuffer(TEXT("next_value"), TEXT("next_index"), TEXT("In_AttributeIndex"));

			OutHLSL += TEXT("\tOut_Impulse = lerp(prev_value, next_value, weight);\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetPointTypeAtTimeName)
	{
		// GetPointTypeAtTime(int In_PointID, float In_Time, out int Out_Type)
		OutHLSL += TEXT("void ") + FunctionInfo.InstanceName + TEXT("(int In_PointID, float In_Time, out int Out_Type) \n{\n");

			OutHLSL += TEXT("\tint In_AttributeIndex = ") + GetSpecAttributeIndex(EHoudiniAttributes::TYPE) + TEXT(";\n");

			OutHLSL += TEXT("\tint prev_index = -1;int next_index = -1;float weight = 1.0f;\n");
			OutHLSL += GetSampleIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));

			OutHLSL += TEXT("\tfloat3 prev_value;\n");
			OutHLSL += ReadFloatInBuffer(TEXT("prev_value"), TEXT("prev_index"), TEXT("In_AttributeIndex"));

			OutHLSL += TEXT("\tOut_Type = floor(prev_value);\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	// END: helper GetPoint<AttrName>AtTime functions, can potentially be removed once get attr by string functions are functional
	}
	else if (FunctionInfo.DefinitionName == GetFloatValueByStringName ||
			 FunctionInfo.DefinitionName == GetVectorValueByStringName ||
			 FunctionInfo.DefinitionName == GetVectorValueExByStringName ||
			 FunctionInfo.DefinitionName == GetVector4ValueByStringName ||
			 FunctionInfo.DefinitionName == GetQuatValueByStringName)
	{
		static const TCHAR *FunctionBodyTemplate = TEXT(
			"void {FunctionName}(int In_SampleIndex, {AdditionalFunctionArguments}out {AttributeType} Out_Value)\n"
			"{\n"
			"	int AttributeIndex = {FunctionIndexToAttributeIndexVarName}[{AttributeFunctionIndex}];\n"
			"	{VectorExFunctionDefaults}\n"
			"   {ReadFromBufferSnippet}\n"
			"}\n\n"
		);

		int AttributeFunctionIndex = -1;
		const bool bFoundAttributeFunctionIndex = GetAttributeFunctionIndex(ParamInfo.GeneratedFunctions, FunctionInstanceIndex, AttributeFunctionIndex);
		if (!bFoundAttributeFunctionIndex)
		{
			UE_LOG(LogHoudiniNiagara, Error, TEXT("Could not determine index for function (counted by Attribute specifier) for '%s'."), *FunctionInfo.InstanceName);
			return false;
		}

		FString AttributeTypeName;
		FString ReadFromBufferSnippet;
		FString AdditionalFunctionArguments;
		FString VectorExFunctionDefaults;
		if (FunctionInfo.DefinitionName == GetFloatValueByStringName)
		{
			AttributeTypeName = "float";
			AdditionalFunctionArguments = "";
			VectorExFunctionDefaults = "";
			ReadFromBufferSnippet = ReadFloatInBuffer(TEXT("Out_Value"), TEXT("In_SampleIndex"), TEXT("AttributeIndex"));
		}
		else if (FunctionInfo.DefinitionName == GetVectorValueByStringName)
		{
			AttributeTypeName = "float3";
			AdditionalFunctionArguments = "";
			VectorExFunctionDefaults = TEXT(
				"	bool In_DoSwap = true;\n"
				"	bool In_DoScale = true;\n"
			);
			ReadFromBufferSnippet = ReadVectorInBuffer(TEXT("Out_Value"), TEXT("In_SampleIndex"), TEXT("AttributeIndex"));
		}
		else if (FunctionInfo.DefinitionName == GetVectorValueExByStringName)
		{
			AttributeTypeName = "float3";
			AdditionalFunctionArguments = TEXT("bool In_DoSwap, bool In_DoScale, ");
			VectorExFunctionDefaults = "";
			ReadFromBufferSnippet = ReadVectorInBuffer(TEXT("Out_Value"), TEXT("In_SampleIndex"), TEXT("AttributeIndex"));
		}
		else if (FunctionInfo.DefinitionName == GetVector4ValueByStringName)
		{
			AttributeTypeName = "float4";
			AdditionalFunctionArguments = "";
			VectorExFunctionDefaults = TEXT(
				"	bool In_DoHoudiniToUnrealConversion = false;\n"
			);
			ReadFromBufferSnippet = ReadVector4InBuffer(TEXT("Out_Value"), TEXT("In_SampleIndex"), TEXT("AttributeIndex"));
		}
		else if (FunctionInfo.DefinitionName == GetQuatValueByStringName)
		{
			AttributeTypeName = "float4";
			AdditionalFunctionArguments = TEXT("bool In_DoHoudiniToUnrealConversion, ");
			VectorExFunctionDefaults = "";
			ReadFromBufferSnippet = ReadVector4InBuffer(TEXT("Out_Value"), TEXT("In_SampleIndex"), TEXT("AttributeIndex"));
		}
		else
		{
			UE_LOG(LogHoudiniNiagara, Error, TEXT("Unexpected function definition name: '%s'"), *FunctionInfo.DefinitionName.ToString());
			return false;
		}

		TMap<FString, FStringFormatArg> FunctionTemplateArgs = {
			{TEXT("FunctionName"), FunctionInfo.InstanceName},
			{TEXT("AttributeType"), AttributeTypeName},
			{TEXT("FunctionIndexToAttributeIndexVarName"), FunctionIndexToAttributeIndexBuffer},
			{TEXT("AttributeFunctionIndex"), AttributeFunctionIndex},
			{TEXT("AdditionalFunctionArguments"), AdditionalFunctionArguments},
			{TEXT("VectorExFunctionDefaults"), VectorExFunctionDefaults},
			{TEXT("ReadFromBufferSnippet"), ReadFromBufferSnippet},
		};

		OutHLSL += FString::Format(FunctionBodyTemplate, FunctionTemplateArgs);
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetPointValueAtTimeByStringName ||
			 FunctionInfo.DefinitionName == GetPointVectorValueAtTimeByStringName ||
			 FunctionInfo.DefinitionName == GetPointVectorValueAtTimeExByStringName ||
			 FunctionInfo.DefinitionName == GetPointVector4ValueAtTimeByStringName ||
			 FunctionInfo.DefinitionName == GetPointQuatValueAtTimeByStringName)
	{
		static const TCHAR *FunctionBodyTemplate = TEXT(
			"void {FunctionName}(int In_PointID, float In_Time, {AdditionalFunctionArguments}out {AttributeType} Out_Value)\n"
			"{\n"
			"	int AttributeIndex = {FunctionIndexToAttributeIndexVarName}[{AttributeFunctionIndex}];\n"
			"	int prev_index = -1;\n"
			"	int next_index = -1;\n"
			"	float weight = 1.0f;\n"
			"	{GetSampleIndexesForPointAtTimeSnippet}\n"
			"\n"
			"	{VectorExFunctionDefaults}\n"
			"	{AttributeType} prev_value;\n"
			"	{AttributeType} next_value;\n"
			"	{ReadPrevFromBufferSnippet}\n"
			"	{ReadNextFromBufferSnippet}\n"

			"	Out_Value = {LerpFunctionName}(prev_value, next_value, weight);\n"
			"}\n\n"
		);

		int AttributeFunctionIndex = -1;
		const bool bFoundAttributeFunctionIndex = GetAttributeFunctionIndex(ParamInfo.GeneratedFunctions, FunctionInstanceIndex, AttributeFunctionIndex);
		if (!bFoundAttributeFunctionIndex)
		{
			UE_LOG(LogHoudiniNiagara, Error, TEXT("Could not determine index for function (counted by Attribute specifier) for '%s'."), *FunctionInfo.InstanceName);
			return false;
		}

		FString AttributeTypeName;
		FString AdditionalFunctionArguments;
		FString GetSampleIndexesForPointAtTimeSnippet;
		FString VectorExFunctionDefaults;
		FString ReadPrevFromBufferSnippet;
		FString ReadNextFromBufferSnippet;
		FString LerpFunctionName;
		if (FunctionInfo.DefinitionName == GetPointValueAtTimeByStringName)
		{
			AttributeTypeName = "float";
			GetSampleIndexesForPointAtTimeSnippet = GetSampleIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));
			AdditionalFunctionArguments = "";
			VectorExFunctionDefaults = "";
			ReadPrevFromBufferSnippet = ReadFloatInBuffer(TEXT("prev_value"), TEXT("prev_index"), TEXT("AttributeIndex"));
			ReadNextFromBufferSnippet = ReadFloatInBuffer(TEXT("next_value"), TEXT("next_index"), TEXT("AttributeIndex"));
			LerpFunctionName = "lerp";
		}
		else if (FunctionInfo.DefinitionName == GetPointVectorValueAtTimeByStringName)
		{
			AttributeTypeName = "float3";
			GetSampleIndexesForPointAtTimeSnippet = GetSampleIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));
			AdditionalFunctionArguments = "";
			VectorExFunctionDefaults = TEXT(
				"	bool In_DoSwap = true;\n"
				"	bool In_DoScale = true;\n"
			);
			ReadPrevFromBufferSnippet = ReadVectorInBuffer(TEXT("prev_value"), TEXT("prev_index"), TEXT("AttributeIndex"));
			ReadNextFromBufferSnippet = ReadVectorInBuffer(TEXT("next_value"), TEXT("next_index"), TEXT("AttributeIndex"));
			LerpFunctionName = "lerp";
		}
		else if (FunctionInfo.DefinitionName == GetPointVectorValueAtTimeExByStringName)
		{
			AttributeTypeName = "float3";
			GetSampleIndexesForPointAtTimeSnippet = GetSampleIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));
			AdditionalFunctionArguments = TEXT("bool In_DoSwap, bool In_DoScale, ");
			VectorExFunctionDefaults = "";
			ReadPrevFromBufferSnippet = ReadVectorInBuffer(TEXT("prev_value"), TEXT("prev_index"), TEXT("AttributeIndex"));
			ReadNextFromBufferSnippet = ReadVectorInBuffer(TEXT("next_value"), TEXT("next_index"), TEXT("AttributeIndex"));
			LerpFunctionName = "lerp";
		}
		else if (FunctionInfo.DefinitionName == GetPointVector4ValueAtTimeByStringName)
		{
			AttributeTypeName = "float4";
			GetSampleIndexesForPointAtTimeSnippet = GetSampleIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));
			AdditionalFunctionArguments = "";
			VectorExFunctionDefaults = TEXT(
				"	bool In_DoHoudiniToUnrealConversion = false;\n"
			);
			ReadPrevFromBufferSnippet = ReadVector4InBuffer(TEXT("prev_value"), TEXT("prev_index"), TEXT("AttributeIndex"));
			ReadNextFromBufferSnippet = ReadVector4InBuffer(TEXT("next_value"), TEXT("next_index"), TEXT("AttributeIndex"));
			LerpFunctionName = "lerp";
		}
		else if (FunctionInfo.DefinitionName == GetPointQuatValueAtTimeByStringName)
		{
			AttributeTypeName = "float4";
			GetSampleIndexesForPointAtTimeSnippet = GetSampleIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));
			AdditionalFunctionArguments = TEXT("bool In_DoHoudiniToUnrealConversion, ");
			VectorExFunctionDefaults = "";
			ReadPrevFromBufferSnippet = ReadVector4InBuffer(TEXT("prev_value"), TEXT("prev_index"), TEXT("AttributeIndex"));
			ReadNextFromBufferSnippet = ReadVector4InBuffer(TEXT("next_value"), TEXT("next_index"), TEXT("AttributeIndex"));
			LerpFunctionName = "q_slerp";
		}
		else
		{
			UE_LOG(LogHoudiniNiagara, Error, TEXT("Unexpected function definition name: '%s'"), *FunctionInfo.DefinitionName.ToString());
			return false;
		}

		TMap<FString, FStringFormatArg> FunctionTemplateArgs = {
			{TEXT("FunctionName"), FunctionInfo.InstanceName},
			{TEXT("AttributeType"), AttributeTypeName},
			{TEXT("FunctionIndexToAttributeIndexVarName"), FunctionIndexToAttributeIndexBuffer},
			{TEXT("AttributeFunctionIndex"), AttributeFunctionIndex},
			{TEXT("GetSampleIndexesForPointAtTimeSnippet"), GetSampleIndexesForPointAtTimeSnippet},
			{TEXT("AdditionalFunctionArguments"), AdditionalFunctionArguments},
			{TEXT("VectorExFunctionDefaults"), VectorExFunctionDefaults},
			{TEXT("ReadPrevFromBufferSnippet"), ReadPrevFromBufferSnippet},
			{TEXT("ReadNextFromBufferSnippet"), ReadNextFromBufferSnippet},
			{TEXT("LerpFunctionName"), LerpFunctionName},
		};

		OutHLSL += FString::Format(FunctionBodyTemplate, FunctionTemplateArgs);
		return true;
	}

	// return !OutHLSL.IsEmpty();
	return false;
}

#endif


#if ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION < 1
#if WITH_EDITORONLY_DATA
void UNiagaraDataInterfaceHoudini::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	// int NumberOfSamples_XX;
	FString BufferName = UNiagaraDataInterfaceHoudini::NumberOfSamplesBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("int ") + BufferName + TEXT(";\n");

	// int NumberOfAttributes_XX;
	BufferName = UNiagaraDataInterfaceHoudini::NumberOfAttributesBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("int ") + BufferName + TEXT(";\n");

	// int NumberOfPoints_XX;
	BufferName = UNiagaraDataInterfaceHoudini::NumberOfPointsBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("int ") + BufferName + TEXT(";\n");

	// Buffer<float> FloatValuesBuffer_XX;
	BufferName = UNiagaraDataInterfaceHoudini::FloatValuesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

	// Buffer<int> SpecialAttributeIndexesBuffer_XX;
	BufferName = UNiagaraDataInterfaceHoudini::SpecialAttributeIndexesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("Buffer<int> ") + BufferName + TEXT(";\n");

	// Buffer<float> SpawnTimesBuffer_XX;
	BufferName = UNiagaraDataInterfaceHoudini::SpawnTimesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

	// Buffer<float> LifeValuesBuffer_XX;
	BufferName = UNiagaraDataInterfaceHoudini::LifeValuesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

	// Buffer<int> PointTypesBuffer_XX;
	BufferName = UNiagaraDataInterfaceHoudini::PointTypesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("Buffer<int> ") + BufferName + TEXT(";\n");

	// int MaxNumberOfIndexesPerPoint_XX;
	BufferName = UNiagaraDataInterfaceHoudini::MaxNumberOfIndexesPerPointBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("int ") + BufferName + TEXT(";\n");

	// Buffer<int> PointValueIndexesBuffer_XX;
	BufferName = UNiagaraDataInterfaceHoudini::PointValueIndexesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("Buffer<int> ") + BufferName + TEXT(";\n");

	// int LastSpawnedPointId_XX;
	BufferName = UNiagaraDataInterfaceHoudini::LastSpawnedPointIdBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("int ") + BufferName + TEXT(";\n");

	// float LastSpawnTime_XX;
	BufferName = UNiagaraDataInterfaceHoudini::LastSpawnTimeBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("float ") + BufferName + TEXT(";\n");

	// float LastSpawnTimeRequest_XX;
	BufferName = UNiagaraDataInterfaceHoudini::LastSpawnTimeRequestBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("float ") + BufferName + TEXT(";\n");

	// int FunctionIndexToAttributeIndexBuffer_XX[#];
	BufferName = UNiagaraDataInterfaceHoudini::FunctionIndexToAttributeIndexBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("Buffer<int> ") + BufferName + TEXT(";\n\n");
}
#endif

#else

#if WITH_EDITORONLY_DATA
bool UNiagaraDataInterfaceHoudini::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	bool bSuccess = Super::AppendCompileHash(InVisitor);
	bSuccess &= InVisitor->UpdateShaderParameters<FShaderParameters>();
	return bSuccess;
}


// Build buffers and member variables HLSL definition
// Always use the BaseName + the DataInterfaceHLSL indentifier!

void UNiagaraDataInterfaceHoudini::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	// int NumberOfSamples_XX;
	FString BufferName = ParamInfo.DataInterfaceHLSLSymbol + UNiagaraDataInterfaceHoudini::NumberOfSamplesBaseName;
	OutHLSL += TEXT("int ") + BufferName + TEXT(";\n");

	// int NumberOfAttributes_XX;
	BufferName = ParamInfo.DataInterfaceHLSLSymbol + UNiagaraDataInterfaceHoudini::NumberOfAttributesBaseName;
	OutHLSL += TEXT("int ") + BufferName + TEXT(";\n");

	// int NumberOfPoints_XX;
	BufferName = ParamInfo.DataInterfaceHLSLSymbol + UNiagaraDataInterfaceHoudini::NumberOfPointsBaseName;
	OutHLSL += TEXT("int ") + BufferName + TEXT(";\n");

	// Buffer<float> FloatValuesBuffer_XX;
	BufferName = ParamInfo.DataInterfaceHLSLSymbol + UNiagaraDataInterfaceHoudini::FloatValuesBufferBaseName;
	OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

	// Buffer<int> SpecialAttributeIndexesBuffer_XX;
	BufferName = ParamInfo.DataInterfaceHLSLSymbol + UNiagaraDataInterfaceHoudini::SpecialAttributeIndexesBufferBaseName;
	OutHLSL += TEXT("Buffer<int> ") + BufferName + TEXT(";\n");

	// Buffer<float> SpawnTimesBuffer_XX;
	BufferName = ParamInfo.DataInterfaceHLSLSymbol + UNiagaraDataInterfaceHoudini::SpawnTimesBufferBaseName;
	OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

	// Buffer<float> LifeValuesBuffer_XX;
	BufferName = ParamInfo.DataInterfaceHLSLSymbol + UNiagaraDataInterfaceHoudini::LifeValuesBufferBaseName;
	OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

	// Buffer<int> PointTypesBuffer_XX;
	BufferName = ParamInfo.DataInterfaceHLSLSymbol + UNiagaraDataInterfaceHoudini::PointTypesBufferBaseName;
	OutHLSL += TEXT("Buffer<int> ") + BufferName + TEXT(";\n");

	// int MaxNumberOfIndexesPerPoint_XX;
	BufferName = ParamInfo.DataInterfaceHLSLSymbol + UNiagaraDataInterfaceHoudini::MaxNumberOfIndexesPerPointBaseName;
	OutHLSL += TEXT("int ") + BufferName + TEXT(";\n");

	// Buffer<int> PointValueIndexesBuffer_XX;
	BufferName = ParamInfo.DataInterfaceHLSLSymbol + UNiagaraDataInterfaceHoudini::PointValueIndexesBufferBaseName;
	OutHLSL += TEXT("Buffer<int> ") + BufferName + TEXT(";\n");

	// int LastSpawnedPointId_XX;
	BufferName = ParamInfo.DataInterfaceHLSLSymbol + UNiagaraDataInterfaceHoudini::LastSpawnedPointIdBaseName;
	OutHLSL += TEXT("int ") + BufferName + TEXT(";\n");

	// float LastSpawnTime_XX;
	BufferName = ParamInfo.DataInterfaceHLSLSymbol + UNiagaraDataInterfaceHoudini::LastSpawnTimeBaseName;
	OutHLSL += TEXT("float ") + BufferName + TEXT(";\n");

	// float LastSpawnTimeRequest_XX;
	BufferName = ParamInfo.DataInterfaceHLSLSymbol + UNiagaraDataInterfaceHoudini::LastSpawnTimeRequestBaseName;
	OutHLSL += TEXT("float ") + BufferName + TEXT(";\n");

	// int FunctionIndexToAttributeIndexBuffer_XX[#];
	BufferName = ParamInfo.DataInterfaceHLSLSymbol + UNiagaraDataInterfaceHoudini::FunctionIndexToAttributeIndexBufferBaseName;
	OutHLSL += TEXT("Buffer<int> ") + BufferName + TEXT(";\n\n");
}
#endif
#endif

//FRWBuffer& UNiagaraDataInterfaceHoudini::GetFloatValuesGPUBuffer()
//{
//	//TODO: This isn't really very thread safe. Need to move to a proxy like system where DIs can push data to the RT safely.
//	if ( FloatValuesGPUBufferDirty )
//	{
//		uint32 NumElements = HoudiniPointCacheAsset ? HoudiniPointCacheAsset->FloatSampleData.Num() : 0;
//		if (NumElements <= 0)
//			return FloatValuesGPUBuffer;
//
//		FloatValuesGPUBuffer.Release();
//		FloatValuesGPUBuffer.Initialize( sizeof(float), NumElements, EPixelFormat::PF_R32_FLOAT, BUF_Static);
//
//		uint32 BufferSize = NumElements * sizeof( float );
//		float* BufferData = static_cast<float*>( RHILockVertexBuffer( FloatValuesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly ) );
//
//		if ( HoudiniPointCacheAsset )
//			FPlatformMemory::Memcpy( BufferData, HoudiniPointCacheAsset->FloatSampleData.GetData(), BufferSize );
//
//		RHIUnlockVertexBuffer( FloatValuesGPUBuffer.Buffer );
//		FloatValuesGPUBufferDirty = false;
//	}
//
//	return FloatValuesGPUBuffer;
//}

//FRWBuffer& UNiagaraDataInterfaceHoudini::GetSpecialAttributeIndexesGPUBuffer()
//{
//	//TODO: This isn't really very thread safe. Need to move to a proxy like system where DIs can push data to the RT safely.
//	if ( SpecialAttributeIndexesGPUBufferDirty )
//	{
//		uint32 NumElements = HoudiniPointCacheAsset ? HoudiniPointCacheAsset->SpecialAttributeIndexes.Num() : 0;
//		if (NumElements <= 0)
//			return SpecialAttributeIndexesGPUBuffer;
//
//		SpecialAttributeIndexesGPUBuffer.Release();
//		SpecialAttributeIndexesGPUBuffer.Initialize(sizeof(int32), NumElements, EPixelFormat::PF_R32_SINT, BUF_Static);
//
//		uint32 BufferSize = NumElements * sizeof(int32);
//		float* BufferData = static_cast<float*>(RHILockVertexBuffer(SpecialAttributeIndexesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
//
//		if (HoudiniPointCacheAsset)
//			FPlatformMemory::Memcpy(BufferData, HoudiniPointCacheAsset->SpecialAttributeIndexes.GetData(), BufferSize);
//
//		RHIUnlockVertexBuffer(SpecialAttributeIndexesGPUBuffer.Buffer);
//		SpecialAttributeIndexesGPUBufferDirty = false;
//	}
//
//	return SpecialAttributeIndexesGPUBuffer;
//}

//FRWBuffer& UNiagaraDataInterfaceHoudini::GetSpawnTimesGPUBuffer()
//{
//	//TODO: This isn't really very thread safe. Need to move to a proxy like system where DIs can push data to the RT safely.
//	if ( SpawnTimesGPUBufferDirty )
//	{
//		uint32 NumElements = HoudiniPointCacheAsset ? HoudiniPointCacheAsset->SpawnTimes.Num() : 0;
//		if (NumElements <= 0)
//			return SpawnTimesGPUBuffer;
//
//		SpawnTimesGPUBuffer.Release();
//		SpawnTimesGPUBuffer.Initialize( sizeof(float), NumElements, EPixelFormat::PF_R32_FLOAT, BUF_Static);
//				
//		uint32 BufferSize = NumElements * sizeof( float );
//		float* BufferData = static_cast<float*>( RHILockVertexBuffer(SpawnTimesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly ) );
//
//		if ( HoudiniPointCacheAsset )
//			FPlatformMemory::Memcpy( BufferData, HoudiniPointCacheAsset->SpawnTimes.GetData(), BufferSize );
//
//		RHIUnlockVertexBuffer( SpawnTimesGPUBuffer.Buffer );
//		SpawnTimesGPUBufferDirty = false;
//	}
//
//	return SpawnTimesGPUBuffer;
//}

//FRWBuffer& UNiagaraDataInterfaceHoudini::GetLifeValuesGPUBuffer()
//{
//	//TODO: This isn't really very thread safe. Need to move to a proxy like system where DIs can push data to the RT safely.
//	if ( LifeValuesGPUBufferDirty )
//	{
//		uint32 NumElements = HoudiniPointCacheAsset ? HoudiniPointCacheAsset->LifeValues.Num() : 0;
//		if (NumElements <= 0)
//			return LifeValuesGPUBuffer;
//
//		LifeValuesGPUBuffer.Release();
//		LifeValuesGPUBuffer.Initialize( sizeof(float), NumElements, EPixelFormat::PF_R32_FLOAT, BUF_Static);
//
//		uint32 BufferSize = NumElements * sizeof( float );
//		float* BufferData = static_cast<float*>( RHILockVertexBuffer( LifeValuesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly ) );
//
//		if ( HoudiniPointCacheAsset )
//			FPlatformMemory::Memcpy( BufferData, HoudiniPointCacheAsset->LifeValues.GetData(), BufferSize );
//
//		RHIUnlockVertexBuffer(LifeValuesGPUBuffer.Buffer );
//		LifeValuesGPUBufferDirty = false;
//	}
//
//	return LifeValuesGPUBuffer;
//}

//FRWBuffer& UNiagaraDataInterfaceHoudini::GetPointTypesGPUBuffer()
//{
//	//TODO: This isn't really very thread safe. Need to move to a proxy like system where DIs can push data to the RT safely.
//	if ( PointTypesGPUBufferDirty )
//	{
//		uint32 NumElements = HoudiniPointCacheAsset ? HoudiniPointCacheAsset->PointTypes.Num() : 0;
//		if (NumElements <= 0)
//			return PointTypesGPUBuffer;
//
//		PointTypesGPUBuffer.Release();
//		PointTypesGPUBuffer.Initialize( sizeof(int32), NumElements, EPixelFormat::PF_R32_SINT, BUF_Static);
//
//		uint32 BufferSize = NumElements * sizeof(int32);
//		int32* BufferData = static_cast<int32*>( RHILockVertexBuffer(PointTypesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly ) );
//
//		if ( HoudiniPointCacheAsset )
//			FPlatformMemory::Memcpy( BufferData, HoudiniPointCacheAsset->PointTypes.GetData(), BufferSize );
//
//		RHIUnlockVertexBuffer(PointTypesGPUBuffer.Buffer );
//		PointTypesGPUBufferDirty = false;
//	}
//
//	return PointTypesGPUBuffer;
//}

//FRWBuffer& UNiagaraDataInterfaceHoudini::GetPointValueIndexesGPUBuffer()
//{
//	//TODO: This isn't really very thread safe. Need to move to a proxy like system where DIs can push data to the RT safely.
//	if ( PointValueIndexesGPUBufferDirty )
//	{
//		uint32 NumPoints = HoudiniPointCacheAsset ? HoudiniPointCacheAsset->PointValueIndexes.Num() : 0;
//		// Add an extra to the max number so all indexes end by a -1
//		uint32 MaxNumIndexesPerPoint = HoudiniPointCacheAsset ? HoudiniPointCacheAsset->GetMaxNumberOfPointValueIndexes() + 1: 0;
//		uint32 NumElements = NumPoints * MaxNumIndexesPerPoint;
//
//		if (NumElements <= 0)
//			return PointValueIndexesGPUBuffer;
//
//		TArray<int32> PointValueIndexes;
//		PointValueIndexes.Init(-1, NumElements);
//		if ( HoudiniPointCacheAsset )
//		{
//			// We need to flatten the nested array for HLSL conversion
//			for ( int32 PointID = 0; PointID < HoudiniPointCacheAsset->PointValueIndexes.Num(); PointID++ )
//			{
//				TArray<int32> SampleIndexes = HoudiniPointCacheAsset->PointValueIndexes[PointID].SampleIndexes;
//				for( int32 Idx = 0; Idx < SampleIndexes.Num(); Idx++ )
//				{
//					PointValueIndexes[ PointID * MaxNumIndexesPerPoint + Idx ] = SampleIndexes[ Idx ];
//				}
//			}
//		}
//
//		PointValueIndexesGPUBuffer.Release();
//		PointValueIndexesGPUBuffer.Initialize(sizeof(int32), NumElements, EPixelFormat::PF_R32_SINT, BUF_Static);
//
//		uint32 BufferSize = NumElements * sizeof(int32);
//		int32* BufferData = static_cast<int32*>( RHILockVertexBuffer( PointValueIndexesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly ) );
//		FPlatformMemory::Memcpy( BufferData, PointValueIndexes.GetData(), BufferSize );
//		RHIUnlockVertexBuffer( PointValueIndexesGPUBuffer.Buffer );
//
//		PointValueIndexesGPUBufferDirty = false;
//	}
//
//	return PointValueIndexesGPUBuffer;
//}

void UNiagaraDataInterfaceHoudini::PushToRenderThreadImpl() 
{
	// Need to throw a ref count into the RHI buffer so that the resource is guaranteed to stay alive while in the queue.
	FHoudiniPointCacheResource* ThisResource = nullptr;
	check(Proxy);
	if (HoudiniPointCacheAsset)
	{
		HoudiniPointCacheAsset->RequestPushToGPU();
		ThisResource = HoudiniPointCacheAsset->Resource.Get();
	}
	
	FNiagaraDataInterfaceProxyHoudini* ThisProxy = GetProxyAs<FNiagaraDataInterfaceProxyHoudini>();
	ENQUEUE_RENDER_COMMAND(FNiagaraDIHoudiniPointCache_ToRT) (
		[ThisProxy, ThisResource](FRHICommandListImmediate& CmdList) mutable
	{
		ThisProxy->bFunctionIndexToAttributeIndexHasBeenBuilt = false; 
		ThisProxy->Resource = ThisResource;
	}
	);
}

FNiagaraDataInterfaceProxyHoudini::FNiagaraDataInterfaceProxyHoudini()
	: FNiagaraDataInterfaceProxy(), Resource(nullptr)
{
	bFunctionIndexToAttributeIndexHasBeenBuilt = false;
}


FNiagaraDataInterfaceProxyHoudini::~FNiagaraDataInterfaceProxyHoudini()
{
}

#if ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION < 1
void FNiagaraDataInterfaceProxyHoudini::UpdateFunctionIndexToAttributeIndexBuffer(const TMemoryImageArray<FName> &FunctionIndexToAttribute, bool bForceUpdate)
#else
void FNiagaraDataInterfaceProxyHoudini::UpdateFunctionIndexToAttributeIndexBuffer(const TMemoryImageArray<FMemoryImageName> &FunctionIndexToAttribute, bool bForceUpdate)
#endif
{
	// Don't rebuild the lookup table if it has already been built and bForceUpdate is false
	if (bFunctionIndexToAttributeIndexHasBeenBuilt && !bForceUpdate)
		return;

	if (!Resource)
		return;

	const uint32 NumFunctions = FunctionIndexToAttribute.Num();
	FunctionIndexToAttributeIndex.SetNum(NumFunctions);
	// For each generated function that uses the Attribute specifier we want to set the attribute index it uses, 
	// or -1 if the Attribute is invalid or missing
	for (uint32 FunctionIndex = 0; FunctionIndex < NumFunctions; ++FunctionIndex)
	{
		const FName &Attribute = FunctionIndexToAttribute[FunctionIndex];
		if (Attribute != NAME_None)
		{
			int32 AttributeIndex = INDEX_NONE;
			if (UHoudiniPointCache::GetAttributeIndexInArrayFromString(Attribute.ToString(), Resource->Attributes, AttributeIndex))
			{
				FunctionIndexToAttributeIndex[FunctionIndex] = AttributeIndex;
			}
			if (AttributeIndex == INDEX_NONE)
			{
				UE_LOG(LogHoudiniNiagara, Warning, TEXT("Trying to access an attribute that does not exist: %s"), *Attribute.ToString());
			}
		}
		else
		{
			FunctionIndexToAttributeIndex[FunctionIndex] = INDEX_NONE;
		}
	}

	const uint32 BufferSize = NumFunctions * sizeof(int32);
	if (FunctionIndexToAttributeIndexGPUBuffer.NumBytes != BufferSize)
	{
		FunctionIndexToAttributeIndexGPUBuffer.Release();
	}

	if (BufferSize > 0)
	{
#if UE_VERSION_OLDER_THAN(5,3,0)
		FunctionIndexToAttributeIndexGPUBuffer.Initialize(TEXT("HoudiniGPUBufferIndexToAttributeIndex"), sizeof(int32), NumFunctions, EPixelFormat::PF_R32_SINT, BUF_Static);
		int32* BufferData = static_cast<int32*>(RHILockBuffer(FunctionIndexToAttributeIndexGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
		FPlatformMemory::Memcpy(BufferData, FunctionIndexToAttributeIndex.GetData(), BufferSize);
		RHIUnlockBuffer(FunctionIndexToAttributeIndexGPUBuffer.Buffer);
#else
		FRHICommandListImmediate& RHICmdList = FRHICommandListImmediate::Get();
		FunctionIndexToAttributeIndexGPUBuffer.Initialize(RHICmdList, TEXT("HoudiniGPUBufferIndexToAttributeIndex"), sizeof(int32), NumFunctions, EPixelFormat::PF_R32_SINT, BUF_Static);
		int32* BufferData = static_cast<int32*>(RHICmdList.LockBuffer(FunctionIndexToAttributeIndexGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
		FPlatformMemory::Memcpy(BufferData, FunctionIndexToAttributeIndex.GetData(), BufferSize);
		RHICmdList.UnlockBuffer(FunctionIndexToAttributeIndexGPUBuffer.Buffer);
#endif
	}

	bFunctionIndexToAttributeIndexHasBeenBuilt = true;
}
#if ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION < 1
// Parameters used for GPU sim compatibility
struct FNiagaraDataInterfaceParametersCS_Houdini : public FNiagaraDataInterfaceParametersCS
{
	DECLARE_TYPE_LAYOUT(FNiagaraDataInterfaceParametersCS_Houdini, NonVirtual);
public:
	void Bind(const FNiagaraDataInterfaceGPUParamInfo& ParameterInfo, const class FShaderParameterMap& ParameterMap)
	{
		NumberOfSamples.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudini::NumberOfSamplesBaseName + ParameterInfo.DataInterfaceHLSLSymbol));
		NumberOfAttributes.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudini::NumberOfAttributesBaseName + ParameterInfo.DataInterfaceHLSLSymbol));
		NumberOfPoints.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudini::NumberOfPointsBaseName + ParameterInfo.DataInterfaceHLSLSymbol));
		
		FloatValuesBuffer.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudini::FloatValuesBufferBaseName + ParameterInfo.DataInterfaceHLSLSymbol));

		SpecialAttributeIndexesBuffer.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudini::SpecialAttributeIndexesBufferBaseName + ParameterInfo.DataInterfaceHLSLSymbol));		

		SpawnTimesBuffer.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudini::SpawnTimesBufferBaseName + ParameterInfo.DataInterfaceHLSLSymbol));
		LifeValuesBuffer.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudini::LifeValuesBufferBaseName + ParameterInfo.DataInterfaceHLSLSymbol));
		PointTypesBuffer.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudini::PointTypesBufferBaseName + ParameterInfo.DataInterfaceHLSLSymbol));

		MaxNumberOfIndexesPerPoint.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudini::MaxNumberOfIndexesPerPointBaseName + ParameterInfo.DataInterfaceHLSLSymbol));
		PointValueIndexesBuffer.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudini::PointValueIndexesBufferBaseName + ParameterInfo.DataInterfaceHLSLSymbol));

		LastSpawnedPointId.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudini::LastSpawnedPointIdBaseName + ParameterInfo.DataInterfaceHLSLSymbol));
		LastSpawnTime.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudini::LastSpawnTimeBaseName + ParameterInfo.DataInterfaceHLSLSymbol));
		LastSpawnTimeRequest.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudini::LastSpawnTimeRequestBaseName + ParameterInfo.DataInterfaceHLSLSymbol));

		FunctionIndexToAttributeIndexBuffer.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudini::FunctionIndexToAttributeIndexBufferBaseName + ParameterInfo.DataInterfaceHLSLSymbol));

		// Build an array of function index -> attribute name (for those functions with the Attribute specifier). If a function does not have the 
		// Attribute specifier, set to NAME_None
		const uint32 NumGeneratedFunctions = ParameterInfo.GeneratedFunctions.Num();
		FunctionIndexToAttribute.Empty(NumGeneratedFunctions);
		const FName NAME_Attribute("Attribute");
		for (const FNiagaraDataInterfaceGeneratedFunction& GeneratedFunction : ParameterInfo.GeneratedFunctions)
		{
			const FName *Attribute = GeneratedFunction.FindSpecifierValue(NAME_Attribute);
			if (Attribute != nullptr)
			{
				FunctionIndexToAttribute.Add(*Attribute);
			}
		}
	}

	void Set(FRHICommandList& RHICmdList, const FNiagaraDataInterfaceSetArgs& Context) const
	{
		check( IsInRenderingThread() );

		FRHIComputeShader* ComputeShaderRHI = Context.Shader.GetComputeShader();
		FNiagaraDataInterfaceProxyHoudini* HoudiniDI = static_cast<FNiagaraDataInterfaceProxyHoudini*>(Context.DataInterface);
		if ( !HoudiniDI)
		{
			return;
		}

		FHoudiniPointCacheResource* Resource = HoudiniDI->Resource;
		if (!Resource)
		{
			return;
		}

		SetShaderValue(RHICmdList, ComputeShaderRHI, NumberOfSamples, Resource->NumSamples);
		SetShaderValue(RHICmdList, ComputeShaderRHI, NumberOfAttributes, Resource->NumAttributes);
		SetShaderValue(RHICmdList, ComputeShaderRHI, NumberOfPoints, Resource->NumPoints);

 		SetSRVParameter(RHICmdList, ComputeShaderRHI, FloatValuesBuffer, Resource->FloatValuesGPUBuffer.SRV);

		SetSRVParameter(RHICmdList, ComputeShaderRHI, SpecialAttributeIndexesBuffer, Resource->SpecialAttributeIndexesGPUBuffer.SRV);

		SetSRVParameter(RHICmdList, ComputeShaderRHI, SpawnTimesBuffer, Resource->SpawnTimesGPUBuffer.SRV);
		SetSRVParameter(RHICmdList, ComputeShaderRHI, LifeValuesBuffer, Resource->LifeValuesGPUBuffer.SRV);
		SetSRVParameter(RHICmdList, ComputeShaderRHI, PointTypesBuffer, Resource->PointTypesGPUBuffer.SRV);

		SetShaderValue(RHICmdList, ComputeShaderRHI, MaxNumberOfIndexesPerPoint, Resource->MaxNumberOfIndexesPerPoint);

		SetSRVParameter(RHICmdList, ComputeShaderRHI, PointValueIndexesBuffer, Resource->PointValueIndexesGPUBuffer.SRV);

		SetShaderValue(RHICmdList, ComputeShaderRHI, LastSpawnedPointId, -1);
		SetShaderValue(RHICmdList, ComputeShaderRHI, LastSpawnTime, -FLT_MAX);
		SetShaderValue(RHICmdList, ComputeShaderRHI, LastSpawnTimeRequest, -FLT_MAX);

		// Build the the function index to attribute index lookup table if it has not yet been built for this DI proxy
		HoudiniDI->UpdateFunctionIndexToAttributeIndexBuffer(FunctionIndexToAttribute);
		
		if (HoudiniDI->FunctionIndexToAttributeIndexGPUBuffer.NumBytes > 0)
		{
			SetSRVParameter(RHICmdList, ComputeShaderRHI, FunctionIndexToAttributeIndexBuffer, HoudiniDI->FunctionIndexToAttributeIndexGPUBuffer.SRV);
		}
		else
		{
			SetSRVParameter(RHICmdList, ComputeShaderRHI, FunctionIndexToAttributeIndexBuffer, FNiagaraRenderer::GetDummyIntBuffer());
		}
	}

private:
	LAYOUT_FIELD(FShaderParameter, NumberOfSamples);
	LAYOUT_FIELD(FShaderParameter, NumberOfAttributes);
	LAYOUT_FIELD(FShaderParameter, NumberOfPoints);

	LAYOUT_FIELD(FShaderResourceParameter, FloatValuesBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, SpecialAttributeIndexesBuffer);

	LAYOUT_FIELD(FShaderResourceParameter, SpawnTimesBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, LifeValuesBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, PointTypesBuffer);

	LAYOUT_FIELD(FShaderParameter, MaxNumberOfIndexesPerPoint);
	LAYOUT_FIELD(FShaderResourceParameter, PointValueIndexesBuffer);

	LAYOUT_FIELD(FShaderParameter, LastSpawnedPointId);
	LAYOUT_FIELD(FShaderParameter, LastSpawnTime);
	LAYOUT_FIELD(FShaderParameter, LastSpawnTimeRequest);

	LAYOUT_FIELD(FShaderResourceParameter, FunctionIndexToAttributeIndexBuffer);

	LAYOUT_FIELD(TMemoryImageArray<FName>, FunctionIndexToAttribute);

	LAYOUT_FIELD_INITIALIZED(uint32, Version, 1);
};

IMPLEMENT_TYPE_LAYOUT(FNiagaraDataInterfaceParametersCS_Houdini);

IMPLEMENT_NIAGARA_DI_PARAMETER(UNiagaraDataInterfaceHoudini, FNiagaraDataInterfaceParametersCS_Houdini);
#endif

#undef LOCTEXT_NAMESPACE
