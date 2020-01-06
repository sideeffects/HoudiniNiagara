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

#include "NiagaraDataInterfaceHoudiniCSV.h"
#include "NiagaraTypes.h"
#include "Misc/FileHelper.h"
#include "NiagaraShader.h"
#include "ShaderParameterUtils.h"

#define LOCTEXT_NAMESPACE "HoudiniNiagaraCSVDataInterface"

// Base name for the member variables / buffers for GPU compatibility
const FString UNiagaraDataInterfaceHoudiniCSV::NumberOfRowsBaseName(TEXT("NumberOfRows_"));
const FString UNiagaraDataInterfaceHoudiniCSV::NumberOfColumnsBaseName(TEXT("NumberOfColumns_"));
const FString UNiagaraDataInterfaceHoudiniCSV::NumberOfPointsBaseName(TEXT("NumberOfPoints_"));
const FString UNiagaraDataInterfaceHoudiniCSV::FloatValuesBufferBaseName(TEXT("FloatValuesBuffer_"));
const FString UNiagaraDataInterfaceHoudiniCSV::SpecialAttributesColumnIndexesBufferBaseName(TEXT("SpecialAttributesColumnIndexesBuffer_"));
const FString UNiagaraDataInterfaceHoudiniCSV::SpawnTimesBufferBaseName(TEXT("SpawnTimesBuffer_"));
const FString UNiagaraDataInterfaceHoudiniCSV::LifeValuesBufferBaseName(TEXT("LifeValuesBuffer_"));
const FString UNiagaraDataInterfaceHoudiniCSV::PointTypesBufferBaseName(TEXT("PointTypesBuffer_"));
const FString UNiagaraDataInterfaceHoudiniCSV::MaxNumberOfIndexesPerPointBaseName(TEXT("MaxNumberOfIndexesPerPoint_"));
const FString UNiagaraDataInterfaceHoudiniCSV::PointValueIndexesBufferBaseName(TEXT("PointValueIndexesBuffer_"));
const FString UNiagaraDataInterfaceHoudiniCSV::LastSpawnedPointIdBaseName(TEXT("LastSpawnedPointId_"));
const FString UNiagaraDataInterfaceHoudiniCSV::LastSpawnTimeBaseName(TEXT("LastSpawnTime_"));


// Name of all the functions available in the data interface
static const FName GetFloatValueName("GetFloatValue");
//static const FName GetFloatValueByStringName("GetFloatValueByString");
static const FName GetVectorValueName("GetVectorValue");
static const FName GetVectorValueExName("GetVectorValueEx");

static const FName GetPositionName("GetPosition");
static const FName GetNormalName("GetNormal");
static const FName GetTimeName("GetTime");
static const FName GetVelocityName("GetVelocity");
static const FName GetColorName("GetColor");
static const FName GetImpulseName("GetImpulse");
static const FName GetPositionAndTimeName("GetPositionAndTime");

static const FName GetNumberOfPointsName("GetNumberOfPoints");
static const FName GetNumberOfRowsName("GetNumberOfRows");
static const FName GetNumberOfColumnsName("GetNumberOfColumns");

static const FName GetLastRowIndexAtTimeName("GetLastRowIndexAtTime");
static const FName GetPointIDsToSpawnAtTimeName("GetPointIDsToSpawnAtTime");
static const FName GetRowIndexesForPointAtTimeName("GetRowIndexesForPointAtTime");

static const FName GetPointPositionAtTimeName("GetPointPositionAtTime");
static const FName GetPointValueAtTimeName("GetPointValueAtTime");
static const FName GetPointVectorValueAtTimeName("GetPointVectorValueAtTime");

static const FName GetPointVectorValueAtTimeExName("GetPointVectorValueAtTimeEx");
static const FName GetPointLifeName("GetPointLife");
//static const FName GetPointLifeAtTimeName("GetPointLifeAtTime");
static const FName GetPointTypeName("GetPointType");

struct FNiagaraDIHoudiniCSV_StaticDataPassToRT
{
	TArray<float>* FloatCSVData;
	TArray<float>* SpawnTimes;
	TArray<float>* LifeValues;
	TArray<int32>* PointTypes;
	TArray<int32>* SpecialAttributesColumnIndexes;
	TArray<int32>* PointValueIndexes;

	int32 NumRows;
	int32 NumColumns;
	int32 NumPoints;
	int32 MaxNumIndexesPerPoint;
};

UNiagaraDataInterfaceHoudiniCSV::UNiagaraDataInterfaceHoudiniCSV(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
    HoudiniCSVAsset = nullptr;
	LastSpawnedPointID = -1;

	Proxy = MakeShared<FNiagaraDataInterfaceProxyHoudiniCSV, ESPMode::ThreadSafe>();
}

void UNiagaraDataInterfaceHoudiniCSV::PostInitProperties()
{
    Super::PostInitProperties();

    if (HasAnyFlags(RF_ClassDefaultObject))
    {
	    FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), true, false, false);
		FNiagaraTypeRegistry::Register(FHoudiniEvent::StaticStruct(), true, true, false);
    }

	LastSpawnedPointID = -1;
	LastSpawnTime = -1.0f;

	PushToRenderThread();
}

void UNiagaraDataInterfaceHoudiniCSV::PostLoad()
{
    Super::PostLoad();

	LastSpawnedPointID = -1;
	LastSpawnTime = -1.0f;

	PushToRenderThread();
}

#if WITH_EDITOR

void UNiagaraDataInterfaceHoudiniCSV::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceHoudiniCSV, HoudiniCSVAsset))
    {
		Modify();
		if ( HoudiniCSVAsset )
		{
			LastSpawnedPointID = -1;
			LastSpawnTime = -1.0f;
			PushToRenderThread();
		}
    }
}

#endif

bool UNiagaraDataInterfaceHoudiniCSV::CopyToInternal(UNiagaraDataInterface* Destination) const
{
    if ( !Super::CopyToInternal( Destination ) )
		return false;

    UNiagaraDataInterfaceHoudiniCSV* CastedInterface = CastChecked<UNiagaraDataInterfaceHoudiniCSV>( Destination );
    if ( !CastedInterface )
		return false;

    CastedInterface->HoudiniCSVAsset = HoudiniCSVAsset;

	CastedInterface->LastSpawnedPointID = -1;
	CastedInterface->LastSpawnTime = -1.0f;
	CastedInterface->PushToRenderThread();

    return true;
}

bool UNiagaraDataInterfaceHoudiniCSV::Equals(const UNiagaraDataInterface* Other) const
{
    if ( !Super::Equals(Other) )
		return false;

    const UNiagaraDataInterfaceHoudiniCSV* OtherHNCSV = CastChecked<UNiagaraDataInterfaceHoudiniCSV>(Other);

    if ( OtherHNCSV != nullptr && OtherHNCSV->HoudiniCSVAsset != nullptr && HoudiniCSVAsset )
    {
		// Just make sure the two interfaces point to the same file
		return OtherHNCSV->HoudiniCSVAsset->FileName.Equals( HoudiniCSVAsset->FileName );
    }

    return false;
}

// Returns the signature of all the functions avaialable in the data interface
void UNiagaraDataInterfaceHoudiniCSV::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
    {
		// GetFloatValue
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetFloatValueName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Col")));			// Col Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));	// Float Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetFloatValue",
			"Returns the float value in the CSV file for a given Row and Column.\n" ) );

		OutFunctions.Add( Sig );
    }

	/*
    {
		// GetFloatValueByString
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetFloatValueByStringName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetStringDef(), TEXT("ColTitle")));	// Col Title In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));	// Float Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetFloatValueByString",
		"Returns the float value in the CSV file for a given Row, in the column corresponding to the ColTitle string.\n" ) );

		OutFunctions.Add(Sig);
    }
	*/

	{
		// GetVectorValue
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetVectorValueName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Col")));			// Col Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));		// Vector3 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetVectorValue",
			"Returns a Vector3 in the CSV file for a given Row and Column.\nThe returned Vector is converted from Houdini's coordinate system to Unreal's." ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetVectorValueEx
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetVectorValueExName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Col")));			// Col Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoSwap")));		// DoSwap in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoScale")));	// DoScale in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));		// Vector3 Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetVectorValueEx",
			"Returns a Vector3 in the CSV file for a given Row and Column.\nThe DoSwap parameter indicates if the vector should be converted from Houdini*s coordinate system to Unreal's.\nThe DoScale parameter decides if the Vector value should be converted from meters (Houdini) to centimeters (Unreal)."));

		OutFunctions.Add(Sig);
	}

    {
		// GetPosition
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPositionName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));	// Vector3 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPosition",
			"Helper function returning the position value for a given Row in the CSV file.\nThe returned Position vector is converted from Houdini's coordinate system to Unreal's." ) );

		OutFunctions.Add(Sig);
    }

    {
		// GetPositionAndTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPositionAndTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));	// Vector3 Out
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		// float Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPositionAndTime",
			"Helper function returning the position and time values for a given Row in the CSV file.\nThe returned Position vector is converted from Houdini's coordinate system to Unreal's." ) );

		OutFunctions.Add(Sig);
    }

    {
		// GetNormal
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetNormalName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));	// Vector3 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetNormal",
			"Helper function returning the normal value for a given Row in the CSV file.\nThe returned Normal vector is converted from Houdini's coordinate system to Unreal's." ) );

		OutFunctions.Add(Sig);
    }

    {
		// GetTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		// Float Out

		Sig.SetDescription( LOCTEXT("DataInterfaceHoudini_GetTime",
			"Helper function returning the time value for a given Row in the CSV file.\n") );

		OutFunctions.Add(Sig);
    }

	{
		// GetVelocity
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetVelocityName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));	// Vector3 Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetVelocity",
			"Helper function returning the velocity value for a given Row in the CSV file.\nThe returned velocity vector is converted from Houdini's coordinate system to Unreal's."));

		OutFunctions.Add(Sig);
	}

	{
		// GetColor
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetColorName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("Color")));	// Color Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetColor",
			"Helper function returning the color value for a given Row in the CSV file."));

		OutFunctions.Add(Sig);
	}

	{
		// GetImpulse
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetImpulseName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Impulse")));	// Float Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetImpulse",
			"Helper function returning the impulse value for a given Row in the CSV file.\n"));

		OutFunctions.Add(Sig);
	}

    {
		// GetNumberOfPoints
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetNumberOfPointsName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));					// CSV in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("NumberOfPoints")));  // Int Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetNumberOfPoints",
			"Returns the number of points (with different id values) in the CSV file.\n" ) );

		OutFunctions.Add(Sig);
    }

	{
		// GetNumberOfRows
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetNumberOfRowsName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add( FNiagaraVariable( FNiagaraTypeDefinition( GetClass()), TEXT("CSV") ) );					// CSV in
		Sig.Outputs.Add( FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("NumberOfRows") ) );		// Int Out
		
		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetNumberOfRows",
			"Returns the number of rows in the CSV file.\nOnly the number of value rows is returned, the first \"Title\" Row is ignored." ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetNumberOfColumns
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetNumberOfColumnsName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add( FNiagaraVariable( FNiagaraTypeDefinition( GetClass()), TEXT("CSV") ) );						// CSV in
		Sig.Outputs.Add( FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("NumberOfColumns") ) );		// Int Out
		
		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetNumberOfColumns",
			"Returns the number of columns in the CSV file." ) );

		OutFunctions.Add(Sig);
	}

    {
		// GetLastRowIndexAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetLastRowIndexAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable( FNiagaraTypeDefinition(GetClass()), TEXT("CSV") ) );					// CSV in
		Sig.Inputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time") ) );				// Time in
		Sig.Outputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("LastRowIndex") ) );	    // Int Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetLastRowIndexAtTime",
			"Returns the index of the last row in the CSV file that has a time value lesser or equal to the Time parameter." ) );

		OutFunctions.Add(Sig);
    }

    {
		// GetPointIdsToSpawnAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointIDsToSpawnAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable( FNiagaraTypeDefinition(GetClass()), TEXT("CSV") ) );				// CSV in
		Sig.Inputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time") ) );		    // Time in
		Sig.Outputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("MinID") ) );			// Int Out
		Sig.Outputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("MaxID") ) );			// Int Out
		Sig.Outputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("Count") ) );		    // Int Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointIDsToSpawnAtTime",
			"Returns the count and point IDs of the points that should spawn for a given time value." ) );

		OutFunctions.Add(Sig);
    }

	{
		// GetRowIndexesForPointAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetRowIndexesForPointAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));					// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));				// Point Number In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));				// Time in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PreviousRow")));		// Int Out
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("NextRow")));			// Int Out
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("PrevWeight")));		// Float Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetRowIndexesForPointAtTime",
			"Returns the row indexes for a given point at a given time.\nThe previous row, next row and weight can then be used to Lerp between values.") );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointPositionAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointPositionAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
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
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In		
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Col")));				// Col Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));		// Float Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointValueAtTime",
			"Returns the linearly interpolated value in the specified column for a given point at a given time." ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointVectorValueAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointVectorValueAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point ID In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Col")));				// Col Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));			// Vector3 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointVectorValueAtTime",
			"Helper function returning the linearly interpolated Vector value in the specified column for a given point at a given time.\nThe returned Vector is converted from Houdini's coordinate system to Unreal's." ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointVectorValueAtTimeEx
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointVectorValueAtTimeExName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point ID In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Col")));				// Col Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoSwap")));			// DoSwap in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoScale")));		// DoScale in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));			// Vector3 Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetPointVectorValueAtTime",
			"Helper function returning the linearly interpolated Vector value in the specified column for a given point at a given time.\nThe DoSwap parameter indicates if the vector should be converted from Houdini*s coordinate system to Unreal's.\nThe DoScale parameter decides if the Vector value should be converted from meters (Houdini) to centimeters (Unreal)." ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointLife
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointLifeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Life")));			// Float Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetPointLife",
			"Helper function returning the life value for a given point when spawned.\nThe life value is either calculated from the alive attribute or is the life attribute at spawn time."));

		OutFunctions.Add(Sig);
	}

	/*
	{
		// GetPointLifeAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointLifeAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In		
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Life")));			// Float Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointLifeAtTime",
			"Helper function returning the remaining life for a given point in the CSV file at a given time." ) );

		OutFunctions.Add(Sig);
	}
	*/

	{
		// GetPointType
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointTypeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Type")));			// Int Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetPointType",
			"Helper function returning the type value for a given point when spawned.\n"));

		OutFunctions.Add(Sig);
	}
}

DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetFloatValue);
//DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetFloatValueByString);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetVectorValue);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetVectorValueEx);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPosition);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetNormal);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetColor);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetVelocity);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetImpulse);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPositionAndTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetLastRowIndexAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointIDsToSpawnAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetRowIndexesForPointAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointPositionAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointValueAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointVectorValueAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointVectorValueAtTimeEx);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointLife);
//DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointLifeAtTime);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointType);
void UNiagaraDataInterfaceHoudiniCSV::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
    if (BindingInfo.Name == GetFloatValueName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
    {
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetFloatValue)::Bind(this, OutFunc);
    }
    /*else if (BindingInfo.Name == GetFloatValueByStringName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
    {
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetFloatValueByString)::Bind(this, BindingInfo, InstanceData, OutFunc);
    }*/
    else if (BindingInfo.Name == GetVectorValueName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
    {
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetVectorValue)::Bind(this, OutFunc);
    }
	else if (BindingInfo.Name == GetVectorValueExName && BindingInfo.GetNumInputs() == 4 && BindingInfo.GetNumOutputs() == 3)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetVectorValueEx)::Bind(this, OutFunc);
	}
    else if (BindingInfo.Name == GetPositionName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
    {
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPosition)::Bind(this, OutFunc);
    }
    else if (BindingInfo.Name == GetNormalName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
    {
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetNormal)::Bind(this, OutFunc);
    }
    else if (BindingInfo.Name == GetTimeName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
    {
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetTime)::Bind(this, OutFunc);
    }
	else if (BindingInfo.Name == GetVelocityName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetVelocity)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetColorName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 4)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetColor)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetImpulseName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetImpulse)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPositionAndTimeName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 4)
    {
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPositionAndTime)::Bind(this, OutFunc);
    }
    else if ( BindingInfo.Name == GetNumberOfPointsName && BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 1 )
    {
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceHoudiniCSV::GetNumberOfPoints);
    }
	else if (BindingInfo.Name == GetNumberOfRowsName && BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 1)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceHoudiniCSV::GetNumberOfRows);
	}
	else if (BindingInfo.Name == GetNumberOfColumnsName && BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 1)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceHoudiniCSV::GetNumberOfColumns);
	}
    else if (BindingInfo.Name == GetLastRowIndexAtTimeName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
    {
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetLastRowIndexAtTime)::Bind(this, OutFunc);
    }
    else if (BindingInfo.Name == GetPointIDsToSpawnAtTimeName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
    {
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointIDsToSpawnAtTime)::Bind(this, OutFunc);
    }
	else if (BindingInfo.Name == GetRowIndexesForPointAtTimeName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetRowIndexesForPointAtTime)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointPositionAtTimeName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointPositionAtTime)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointValueAtTimeName && BindingInfo.GetNumInputs() == 3 && BindingInfo.GetNumOutputs() == 1)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointValueAtTime)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointVectorValueAtTimeName && BindingInfo.GetNumInputs() == 3 && BindingInfo.GetNumOutputs() == 3)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointVectorValueAtTime)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointVectorValueAtTimeExName && BindingInfo.GetNumInputs() == 5 && BindingInfo.GetNumOutputs() == 3)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointVectorValueAtTimeEx)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetPointLifeName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointLife)::Bind(this, OutFunc);
	}
	/*
	else if (BindingInfo.Name == GetPointLifeAtTimeName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointLifeAtTime)::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	*/
	else if (BindingInfo.Name == GetPointTypeName && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointType)::Bind(this, OutFunc);
	}
    else
    {
		UE_LOG( LogHoudiniNiagara, Error, 
	    TEXT( "Could not find data interface function:\n\tName: %s\n\tInputs: %i\n\tOutputs: %i" ),
	    *BindingInfo.Name.ToString(), BindingInfo.GetNumInputs(), BindingInfo.GetNumOutputs() );
		OutFunc = FVMExternalFunction();
    }
}

void UNiagaraDataInterfaceHoudiniCSV::GetFloatValue(FVectorVMContext& Context)
{
    VectorVM::FExternalFuncInputHandler<int32> RowParam(Context);
    VectorVM::FExternalFuncInputHandler<int32> ColParam(Context);

    VectorVM::FExternalFuncRegisterHandler<float> OutValue(Context);

    for ( int32 i = 0; i < Context.NumInstances; ++i )
    {
		int32 row = RowParam.Get();
		int32 col = ColParam.Get();
	
		float value = 0.0f;
		if ( HoudiniCSVAsset )
			HoudiniCSVAsset->GetFloatValue( row, col, value );

		*OutValue.GetDest() = value;
		RowParam.Advance();
		ColParam.Advance();
		OutValue.Advance();
    }
}

void UNiagaraDataInterfaceHoudiniCSV::GetVectorValue( FVectorVMContext& Context )
{
    VectorVM::FExternalFuncInputHandler<int32> RowParam(Context);
    VectorVM::FExternalFuncInputHandler<int32> ColParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutVectorX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorZ(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 row = RowParam.Get();
		int32 col = ColParam.Get();

		FVector V = FVector::ZeroVector;
		if ( HoudiniCSVAsset )
			HoudiniCSVAsset->GetVectorValue(row, col, V);

		*OutVectorX.GetDest() = V.X;
		*OutVectorY.GetDest() = V.Y;
		*OutVectorZ.GetDest() = V.Z;

		RowParam.Advance();
		ColParam.Advance();
		OutVectorX.Advance();
		OutVectorY.Advance();
		OutVectorZ.Advance();
    }
}

void UNiagaraDataInterfaceHoudiniCSV::GetVectorValueEx(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> RowParam(Context);
	VectorVM::FExternalFuncInputHandler<int32> ColParam(Context);
	VectorVM::FExternalFuncInputHandler<FNiagaraBool> DoSwapParam(Context);
	VectorVM::FExternalFuncInputHandler<FNiagaraBool> DoScaleParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutVectorX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVectorZ(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 row = RowParam.Get();
		int32 col = ColParam.Get();

		bool DoSwap = DoSwapParam.Get().GetValue();
		bool DoScale = DoScaleParam.Get().GetValue();

		FVector V = FVector::ZeroVector;
		if (HoudiniCSVAsset)
			HoudiniCSVAsset->GetVectorValue(row, col, V, DoSwap, DoScale);

		*OutVectorX.GetDest() = V.X;
		*OutVectorY.GetDest() = V.Y;
		*OutVectorZ.GetDest() = V.Z;

		RowParam.Advance();
		ColParam.Advance();
		DoSwapParam.Advance();
		DoScaleParam.Advance();
		OutVectorX.Advance();
		OutVectorY.Advance();
		OutVectorZ.Advance();
	}
}
/*
void UNiagaraDataInterfaceHoudiniCSV::GetFloatValueByString(FVectorVMContext& Context)
{
    VectorVM::FExternalFuncInputHandler<int32> RowParam(Context);
    ColTitleParamType ColTitleParam(Context);

    VectorVM::FExternalFuncRegisterHandler<float> OutValue(Context);

    for ( int32 i = 0; i < Context.NumInstances; ++i )
    {
		int32 row = RowParam.Get();
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
*/

void UNiagaraDataInterfaceHoudiniCSV::GetPosition(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> RowParam(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutSampleX(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutSampleY(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutSampleZ(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 row = RowParam.Get();

		FVector V = FVector::ZeroVector;
		if ( HoudiniCSVAsset )
			HoudiniCSVAsset->GetPositionValue( row, V );

		*OutSampleX.GetDest() = V.X;
		*OutSampleY.GetDest() = V.Y;
		*OutSampleZ.GetDest() = V.Z;
		RowParam.Advance();
		OutSampleX.Advance();
		OutSampleY.Advance();
		OutSampleZ.Advance();
    }
}

void UNiagaraDataInterfaceHoudiniCSV::GetNormal(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> RowParam(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutSampleX(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutSampleY(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutSampleZ(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 row = RowParam.Get();

		FVector V = FVector::ZeroVector;
		if ( HoudiniCSVAsset )
			HoudiniCSVAsset->GetNormalValue( row, V );

		*OutSampleX.GetDest() = V.X;
		*OutSampleY.GetDest() = V.Y;
		*OutSampleZ.GetDest() = V.Z;
		RowParam.Advance();
		OutSampleX.Advance();
		OutSampleY.Advance();
		OutSampleZ.Advance();
    }
}

void UNiagaraDataInterfaceHoudiniCSV::GetTime(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> RowParam(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutValue(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 row = RowParam.Get();

		float value = 0.0f;
		if ( HoudiniCSVAsset )
			HoudiniCSVAsset->GetTimeValue( row, value );

		*OutValue.GetDest() = value;
		RowParam.Advance();
		OutValue.Advance();
    }
}

void UNiagaraDataInterfaceHoudiniCSV::GetVelocity(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> RowParam(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleZ(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 row = RowParam.Get();

		FVector V = FVector::ZeroVector;
		if ( HoudiniCSVAsset )
			HoudiniCSVAsset->GetVelocityValue( row, V );

		*OutSampleX.GetDest() = V.X;
		*OutSampleY.GetDest() = V.Y;
		*OutSampleZ.GetDest() = V.Z;
		RowParam.Advance();
		OutSampleX.Advance();
		OutSampleY.Advance();
		OutSampleZ.Advance();
	}
}

void UNiagaraDataInterfaceHoudiniCSV::GetColor(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> RowParam(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleR(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleG(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleB(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleA(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 row = RowParam.Get();

		FLinearColor C = FLinearColor::White;
		if ( HoudiniCSVAsset )
			HoudiniCSVAsset->GetColorValue( row, C );

		*OutSampleR.GetDest() = C.R;
		*OutSampleG.GetDest() = C.G;
		*OutSampleB.GetDest() = C.B;
		*OutSampleA.GetDest() = C.A;
		RowParam.Advance();
		OutSampleR.Advance();
		OutSampleG.Advance();
		OutSampleB.Advance();
		OutSampleA.Advance();
	}
}

void UNiagaraDataInterfaceHoudiniCSV::GetImpulse(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> RowParam(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutValue(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 row = RowParam.Get();

		float value = 0.0f;
		if (HoudiniCSVAsset)
			HoudiniCSVAsset->GetImpulseValue(row, value);

		*OutValue.GetDest() = value;
		RowParam.Advance();
		OutValue.Advance();
	}
}

// Returns the last index of the points that should be spawned at time t
void UNiagaraDataInterfaceHoudiniCSV::GetLastRowIndexAtTime(FVectorVMContext& Context)
{
    VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);
    VectorVM::FExternalFuncRegisterHandler<int32> OutValue(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		float t = TimeParam.Get();

		int32 value = 0;
		if ( HoudiniCSVAsset )
			HoudiniCSVAsset->GetLastRowIndexAtTime( t, value );

		*OutValue.GetDest() = value;
		TimeParam.Advance();
		OutValue.Advance();
    }
}

// Returns the last index of the points that should be spawned at time t
void UNiagaraDataInterfaceHoudiniCSV::GetPointIDsToSpawnAtTime( FVectorVMContext& Context )
{
    VectorVM::FExternalFuncInputHandler<float> TimeParam( Context );
    VectorVM::FExternalFuncRegisterHandler<int32> OutMinValue( Context );
    VectorVM::FExternalFuncRegisterHandler<int32> OutMaxValue( Context );
    VectorVM::FExternalFuncRegisterHandler<int32> OutCountValue( Context );

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		float t = TimeParam.Get();

		int32 value = 0;
		int32 min = 0, max = 0, count = 0;

		if ( HoudiniCSVAsset )
		{
			HoudiniCSVAsset->GetPointIDsToSpawnAtTime(t, min, max, count, LastSpawnedPointID, LastSpawnTime);
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

void UNiagaraDataInterfaceHoudiniCSV::GetPositionAndTime(FVectorVMContext& Context)
{
    VectorVM::FExternalFuncInputHandler<int32> RowParam(Context);

    VectorVM::FExternalFuncRegisterHandler<float> OutPosX(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutPosY(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutPosZ(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutTime(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 row = RowParam.Get();

		float timeValue = 0.0f;
		FVector posVector = FVector::ZeroVector;
		if ( HoudiniCSVAsset )
		{
			HoudiniCSVAsset->GetTimeValue( row, timeValue);
			HoudiniCSVAsset->GetPositionValue( row, posVector);
		}

		*OutPosX.GetDest() = posVector.X;
		*OutPosY.GetDest() = posVector.Y;
		*OutPosZ.GetDest() = posVector.Z;

		*OutTime.GetDest() = timeValue;

		RowParam.Advance();
		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
		OutTime.Advance();
    }
}

void UNiagaraDataInterfaceHoudiniCSV::GetRowIndexesForPointAtTime(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutPrevIndex(Context);
	VectorVM::FExternalFuncRegisterHandler<int32> OutNextIndex(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutWeightValue(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 PointID = PointIDParam.Get();
		float time = TimeParam.Get();

		float weight = 0.0f;
		int32 prevIdx = 0;
		int32 nextIdx = 0;
		if ( HoudiniCSVAsset )
		{
			HoudiniCSVAsset->GetRowIndexesForPointAtTime( PointID, time, prevIdx, nextIdx, weight );
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

void UNiagaraDataInterfaceHoudiniCSV::GetPointPositionAtTime( FVectorVMContext& Context )
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosZ(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 PointID = PointIDParam.Get();
		float time = TimeParam.Get();

		FVector posVector = FVector::ZeroVector;
		if ( HoudiniCSVAsset )
		{
			HoudiniCSVAsset->GetPointPositionAtTime(PointID, time, posVector);
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

void UNiagaraDataInterfaceHoudiniCSV::GetPointValueAtTime(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);
	VectorVM::FExternalFuncInputHandler<int32> ColParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutValue(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 PointID = PointIDParam.Get();
		int32 Col = ColParam.Get();
		float time = TimeParam.Get();		

		float Value = 0.0f;
		if ( HoudiniCSVAsset )
		{
			HoudiniCSVAsset->GetPointValueAtTime( PointID, Col, time, Value );
		}

		*OutValue.GetDest() = Value;

		PointIDParam.Advance();
		ColParam.Advance();
		TimeParam.Advance();

		OutValue.Advance();
	}
}

void UNiagaraDataInterfaceHoudiniCSV::GetPointVectorValueAtTime(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<int32> ColParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);	

	VectorVM::FExternalFuncRegisterHandler<float> OutPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosZ(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 PointID = PointIDParam.Get();
		int32 Col = ColParam.Get();
		float time = TimeParam.Get();		

		FVector posVector = FVector::ZeroVector;
		if ( HoudiniCSVAsset )
		{
			HoudiniCSVAsset->GetPointVectorValueAtTime( PointID, Col, time, posVector, true, true);
		}

		*OutPosX.GetDest() = posVector.X;
		*OutPosY.GetDest() = posVector.Y;
		*OutPosZ.GetDest() = posVector.Z;

		PointIDParam.Advance();
		ColParam.Advance();
		TimeParam.Advance();
		
		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
	}
}

void UNiagaraDataInterfaceHoudiniCSV::GetPointVectorValueAtTimeEx(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<int32> ColParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);
	VectorVM::FExternalFuncInputHandler<FNiagaraBool> DoSwapParam(Context);
	VectorVM::FExternalFuncInputHandler<FNiagaraBool> DoScaleParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosZ(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 PointID = PointIDParam.Get();
		int32 Col = ColParam.Get();
		float time = TimeParam.Get();

		bool DoSwap = DoSwapParam.Get().GetValue();
		bool DoScale = DoScaleParam.Get().GetValue();

		FVector posVector = FVector::ZeroVector;
		if (HoudiniCSVAsset)
		{
			HoudiniCSVAsset->GetPointVectorValueAtTime(PointID, Col, time, posVector, DoSwap, DoScale);
		}

		*OutPosX.GetDest() = posVector.X;
		*OutPosY.GetDest() = posVector.Y;
		*OutPosZ.GetDest() = posVector.Z;

		PointIDParam.Advance();
		ColParam.Advance();
		TimeParam.Advance();
		DoSwapParam.Advance();
		DoScaleParam.Advance();

		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
	}
}

void UNiagaraDataInterfaceHoudiniCSV::GetPointLife(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutValue(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 PointID = PointIDParam.Get();

		float Value = 0.0f;
		if ( HoudiniCSVAsset )
		{
			HoudiniCSVAsset->GetPointLife(PointID, Value);
		}

		*OutValue.GetDest() = Value;

		PointIDParam.Advance();

		OutValue.Advance();
	}
}

/*
template<typename VectorVM::FExternalFuncInputHandler<int32>, typename VectorVM::FExternalFuncInputHandler<float>>
void UNiagaraDataInterfaceHoudiniCSV::GetPointLifeAtTime(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);
	VectorVM::FExternalFuncInputHandler<float> TimeParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutValue(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 PointID = PointIDParam.Get();
		float time = TimeParam.Get();

		float Value = 0.0f;
		if ( HoudiniCSVAsset )
		{
			HoudiniCSVAsset->GetPointLifeAtTime(PointID, time, Value);
		}

		*OutValue.GetDest() = Value;

		PointIDParam.Advance();
		TimeParam.Advance();

		OutValue.Advance();
	}
}
*/

void UNiagaraDataInterfaceHoudiniCSV::GetPointType(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> PointIDParam(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutValue(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 PointID = PointIDParam.Get();

		int32 Value = 0;
		if (HoudiniCSVAsset)
		{
			HoudiniCSVAsset->GetPointType(PointID, Value);
		}

		*OutValue.GetDest() = Value;

		PointIDParam.Advance();

		OutValue.Advance();
	}
}

void UNiagaraDataInterfaceHoudiniCSV::GetNumberOfRows(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncRegisterHandler<int32> OutNumRows(Context);
	*OutNumRows.GetDest() = HoudiniCSVAsset ? HoudiniCSVAsset->GetNumberOfRows() : 0;
	OutNumRows.Advance();
}

void UNiagaraDataInterfaceHoudiniCSV::GetNumberOfColumns(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncRegisterHandler<int32> OutNumCols(Context);
	*OutNumCols.GetDest() = HoudiniCSVAsset ? HoudiniCSVAsset->GetNumberOfColumns() : 0;
	OutNumCols.Advance();
}

void UNiagaraDataInterfaceHoudiniCSV::GetNumberOfPoints(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncRegisterHandler<int32> OutNumPoints(Context);
	*OutNumPoints.GetDest() = HoudiniCSVAsset ? HoudiniCSVAsset->GetNumberOfPoints() : 0;
	OutNumPoints.Advance();
}

// Build the shader function HLSL Code.
bool UNiagaraDataInterfaceHoudiniCSV::GetFunctionHLSL(const FName& DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	// Build the buffer/variable names declared for this DI
	FString NumberOfRowsVar = NumberOfRowsBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	FString NumberOfColumnsVar = NumberOfColumnsBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	FString NumberOfPointsVar = NumberOfPointsBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	FString FloatBufferVar = FloatValuesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	FString AttributeColumnIndexesBuffer = SpecialAttributesColumnIndexesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	FString SpawnTimeBuffer = SpawnTimesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	FString LifeValuesBuffer = LifeValuesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	FString PointTypesBuffer = PointTypesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	FString MaxNumberOfIndexesPerPointVar = MaxNumberOfIndexesPerPointBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	FString PointValueIndexesBuffer = PointValueIndexesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	FString LastSpawnedPointIDVar = LastSpawnedPointIdBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	FString LastSpawnTimeVar = LastSpawnTimeBaseName + ParamInfo.DataInterfaceHLSLSymbol;

	// Lambda returning the HLSL code used for reading a Float value in the FloatBuffer
	auto ReadFloatInBuffer = [&](const FString& OutFloatValue, const FString& FloatRowIndex, const FString& FloatColIndex)
	{
		// \t OutValue = FloatBufferName[ (RowIndex) + ( (ColIndex) * (NumberOfRowsName) ) ];\n
		return TEXT("\t ") + OutFloatValue + TEXT(" = ") + FloatBufferVar + TEXT("[ (") + FloatRowIndex + TEXT(") + ( (") + FloatColIndex + TEXT(") * (") + NumberOfRowsVar + TEXT(") ) ];\n");
	};

	// Lambda returning the HLSL code for reading a Vector value in the FloatBuffer
	// It expects the In_DoSwap and In_DoScale integers to be defined before being called!
	auto ReadVectorInBuffer = [&](const FString& OutVectorValue, const FString& VectorRowIndex, const FString& VectorColIndex)
	{
		FString OutHLSLCode;
		OutHLSLCode += TEXT("\t// ReadVectorInBuffer\n");
		OutHLSLCode += TEXT("\t{\n");
			OutHLSLCode += TEXT("\t\tfloat3 temp_Value = float3(0.0, 0.0, 0.0);\n");
			OutHLSLCode += ReadFloatInBuffer(TEXT("temp_Value.x"), VectorRowIndex, VectorColIndex);
			OutHLSLCode += ReadFloatInBuffer(TEXT("temp_Value.y"), VectorRowIndex, VectorColIndex + TEXT(" + 1"));
			OutHLSLCode += ReadFloatInBuffer(TEXT("temp_Value.z"), VectorRowIndex, VectorColIndex + TEXT(" + 2"));
			OutHLSLCode += TEXT("\t\t") + OutVectorValue + TEXT(" = temp_Value;\n");
			OutHLSLCode += TEXT("\t\tif ( In_DoSwap == 1 )\n");
			OutHLSLCode += TEXT("\t\t{\n");
				OutHLSLCode += TEXT("\t\t\t") + OutVectorValue + TEXT(".y= temp_Value.z;\n");
				OutHLSLCode += TEXT("\t\t\t") + OutVectorValue + TEXT(".z= temp_Value.y;\n");
			OutHLSLCode += TEXT("\t\t}\n");
			OutHLSLCode += TEXT("\t\tif ( In_DoScale == 1 )\n");
			OutHLSLCode += TEXT("\t\t{\n");
				OutHLSLCode += TEXT("\t\t\t") + OutVectorValue + TEXT(" *= 100.0f;\n");
			OutHLSLCode += TEXT("\t\t}\n");
		OutHLSLCode += TEXT("\t}\n");
		return OutHLSLCode;
	};

	// Lambda returning the HLSL code for reading a special attribute's column index in the buffer
	auto GetSpecAttributeColumnIndex = [&](const EHoudiniAttributes& Attr)
	{
		FString OutHLSLCode;
		OutHLSLCode += AttributeColumnIndexesBuffer + TEXT("[") + FString::FromInt(Attr) + TEXT("]");
		return OutHLSLCode;
	};

	// Lambda returning the HLSL code for getting the previous and next row indexes for a given particle at a given time	
	auto GetRowIndexesForPointAtTime = [&](const FString& In_PointID, const FString& In_Time, const FString& Out_PreviousRow, const FString& Out_NextRow, const FString& Out_Weight)
	{
		FString OutHLSLCode;
		OutHLSLCode += TEXT("\t// GetRowIndexesForPointAtTime\n");
		OutHLSLCode += TEXT("\t{\n");

			OutHLSLCode += TEXT("\t\tfloat prev_time = -1.0f;\n");
			OutHLSLCode += TEXT("\t\tfloat next_time = -1.0f;\n");

			// Look at all the values for this Point
			OutHLSLCode += TEXT("\t\tfor( int n = 0; n < ") + MaxNumberOfIndexesPerPointVar + TEXT("; n++ )\n\t{\n");
				OutHLSLCode += TEXT("\t\t\tint current_row = ") + PointValueIndexesBuffer + TEXT("[ (") + In_PointID + TEXT(") * ") + MaxNumberOfIndexesPerPointVar + TEXT(" + n ];\n");
				OutHLSLCode += TEXT("\t\t\tif ( current_row < 0 ){ break; }\n");

				OutHLSLCode += TEXT("\t\t\tfloat current_time = -1.0f;\n");
				OutHLSLCode += TEXT("\t\t\tint time_col = ") + GetSpecAttributeColumnIndex(EHoudiniAttributes::TIME) + TEXT(";\n");
				OutHLSLCode += TEXT("\t\t\tif ( time_col < 0 || time_col >= ") + NumberOfColumnsVar + TEXT(" )\n");
					OutHLSLCode += TEXT("\t\t\t\t{ current_time = 0.0f; }\n");
				OutHLSLCode += TEXT("\t\t\telse\n");
					OutHLSLCode += TEXT("\t\t\t{") + ReadFloatInBuffer(TEXT("current_time"), TEXT("current_row"), TEXT("time_col")) + TEXT(" }\n");

				OutHLSLCode += TEXT("\t\t\tif ( current_time == (") + In_Time + TEXT(") )\n");
					OutHLSLCode += TEXT("\t\t\t\t{ ") + Out_PreviousRow + TEXT(" = current_row; ") + Out_NextRow + TEXT(" = current_row; ") + Out_Weight + TEXT(" = 1.0; return;}\n");
				OutHLSLCode += TEXT("\t\t\telse if ( current_time < (") + In_Time + TEXT(") ){\n");
					OutHLSLCode += TEXT("\t\t\t\tif ( prev_time == -1.0f || prev_time < current_time )\n");
						OutHLSLCode += TEXT("\t\t\t\t\t { ") + Out_PreviousRow + TEXT(" = current_row; prev_time = current_time; }\n");
				OutHLSLCode += TEXT("\t\t\t}\n");
				OutHLSLCode += TEXT("\t\t\telse if ( next_time == -1.0f || next_time > current_time )\n");
					OutHLSLCode += TEXT("\t\t\t\t { ") + Out_NextRow + TEXT(" = current_row; next_time = current_time; break; }\n");
			OutHLSLCode += TEXT("\t\t}\n");

			OutHLSLCode += TEXT("\t\tif ( ") + Out_PreviousRow + TEXT(" < 0 )\n");
				OutHLSLCode += TEXT("\t\t\t{ ") + Out_Weight + TEXT(" = 0.0f; ") + Out_PreviousRow + TEXT(" = ") + Out_NextRow + TEXT("; return;}\n");
			OutHLSLCode += TEXT("\t\tif ( ") + Out_NextRow + TEXT(" < 0 )\n");
				OutHLSLCode += TEXT("\t\t\t{ ") + Out_Weight + TEXT(" = 1.0f; ") + Out_NextRow + TEXT(" = ") + Out_PreviousRow + TEXT("; return;}\n");

				// Calculate the weight
			OutHLSLCode += TEXT("\t\t") + Out_Weight + TEXT(" = ( ( (") + In_Time + TEXT(") - prev_time ) / ( next_time - prev_time ) );\n");

		OutHLSLCode += TEXT("\t}\n");
		return OutHLSLCode;
	};

	// Build each function's HLSL code
	if (DefinitionFunctionName == GetFloatValueName)
	{
		// GetFloatValue(int In_Row, int In_Col, out float Out_Value)		
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_Row, int In_Col, out float Out_Value) \n{\n");

			OutHLSL += ReadFloatInBuffer(TEXT("Out_Value"), TEXT("In_Row"), TEXT("In_Col"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	/*
	else if (DefinitionFunctionName == GetFloatValueByStringName)
	{
		// GetFloatValueByString(int In_Row, string In_ColTitle, out float Out_Value)
		FString BufferName = FloatValuesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_Row, string In_ColTitle, out float Out_Value) \n{\n");
		// NEED CODE
		OutHLSL += TEXT("\n}\n");
		return true;
	}
	*/
	else if (DefinitionFunctionName == GetVectorValueName)
	{
		// GetVectorValue(int In_Row, int In_Col, out float3 Out_Value)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_Row, int In_Col, out float3 Out_Value) \n{\n");

			OutHLSL += TEXT("\tint In_DoSwap = 1;\n");
			OutHLSL += TEXT("\tint In_DoScale = 1;\n");
			OutHLSL += ReadVectorInBuffer(TEXT("Out_Value"), TEXT("In_Row"), TEXT("In_Col"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == GetVectorValueExName)
	{
		// GetVectorValueEx(int In_Row, int In_Col, int In_DoSwap, int In_DoScale, out float3 Out_Value)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_Row, int In_Col, int In_DoSwap, int In_DoScale, out float3 Out_Value) \n{\n");
		
			OutHLSL += ReadVectorInBuffer(TEXT("Out_Value"), TEXT("In_Row"), TEXT("In_Col"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == GetPositionName)
	{
		// GetPosition(int In_Row, out float3 Out_Position)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_Row, out float3 Out_Position) \n{\n");

			OutHLSL += TEXT("\tint In_DoSwap = 1;\n");
			OutHLSL += TEXT("\tint In_DoScale = 1;\n");
			OutHLSL += TEXT("\tint In_Col = ") + GetSpecAttributeColumnIndex( EHoudiniAttributes::POSITION ) + TEXT(";\n");
			OutHLSL += ReadVectorInBuffer(TEXT("Out_Position"), TEXT("In_Row"), TEXT("In_Col"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == GetPositionAndTimeName)
	{
		// GetPositionAndTime(int In_Row, out float3 Out_Position, out float Out_Time)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_Row, out float3 Out_Position, out float Out_Time) \n{\n");

			OutHLSL += TEXT("\tint In_DoSwap = 1;\n");
			OutHLSL += TEXT("\tint In_DoScale = 1;\n");
			OutHLSL += TEXT("\tint In_PosCol = ") + GetSpecAttributeColumnIndex(EHoudiniAttributes::POSITION) + TEXT(";\n");
			OutHLSL += ReadVectorInBuffer(TEXT("Out_Position"), TEXT("In_Row"), TEXT("In_PosCol"));

			OutHLSL += TEXT("\tint In_TimeCol = ") + GetSpecAttributeColumnIndex(EHoudiniAttributes::TIME) + TEXT(";\n");
			OutHLSL += ReadFloatInBuffer(TEXT("Out_Time"), TEXT("In_Row"), TEXT("In_TimeCol"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == GetNormalName)
	{
		// GetNormal(int In_Row, out float3 Out_Value)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_Row, out float3 Out_Normal) \n{\n");

			OutHLSL += TEXT("\tint In_DoSwap = 1;\n");
			OutHLSL += TEXT("\tint In_DoScale = 0;\n");
			OutHLSL += TEXT("\tint In_Col = ") + GetSpecAttributeColumnIndex(EHoudiniAttributes::NORMAL) + TEXT(";\n");
			OutHLSL += ReadVectorInBuffer(TEXT("Out_Normal"), TEXT("In_Row"), TEXT("In_Col"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == GetTimeName)
	{
		// GetTime(int In_Row, out float Out_Value)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_Row, out float Out_Time) \n{\n");

			OutHLSL += TEXT("\tint In_TimeCol = ") + GetSpecAttributeColumnIndex(EHoudiniAttributes::TIME) + TEXT(";\n");
			OutHLSL += ReadFloatInBuffer(TEXT("Out_Time"), TEXT("In_Row"), TEXT("In_TimeCol"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == GetVelocityName)
	{
		// GetVelocity(int In_Row, out float3 Out_Value)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_Row, out float3 Out_Velocity) \n{\n");

			OutHLSL += TEXT("\tint In_DoSwap = 1;\n");
			OutHLSL += TEXT("\tint In_DoScale = 0;\n");
			OutHLSL += TEXT("\tint In_Col = ") + GetSpecAttributeColumnIndex(EHoudiniAttributes::VELOCITY) + TEXT(";\n");
			OutHLSL += ReadVectorInBuffer(TEXT("Out_Velocity"), TEXT("In_Row"), TEXT("In_Col"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == GetImpulseName)
	{
		// GetImpulse(int In_Row, out float Out_Value)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_Row, out float Out_Impulse) \n{\n");

		OutHLSL += TEXT("\tint In_ImpulseCol = ") + GetSpecAttributeColumnIndex(EHoudiniAttributes::IMPULSE) + TEXT(";\n");
		OutHLSL += ReadFloatInBuffer(TEXT("Out_Impulse"), TEXT("In_Row"), TEXT("In_ImpulseCol"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == GetColorName)
	{
		// GetColor(int In_Row, out float4 Out_Value)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_Row, out float4 Out_Color) \n{\n");

			OutHLSL += TEXT("\tfloat3 temp_color = float3(1.0, 1.0, 1.0);\n");
			OutHLSL += TEXT("\tint In_ColorCol = ") + GetSpecAttributeColumnIndex(EHoudiniAttributes::COLOR) + TEXT(";\n");
			OutHLSL += TEXT("\tint In_DoSwap = 0;\n");
			OutHLSL += TEXT("\tint In_DoScale = 0;\n");
			OutHLSL += ReadVectorInBuffer(TEXT("temp_color"), TEXT("In_Row"), TEXT("In_ColorCol"));

			OutHLSL += TEXT("\tfloat temp_alpha = 1.0;\n");
			OutHLSL += TEXT("\tint In_TimeCol = ") + GetSpecAttributeColumnIndex(EHoudiniAttributes::TIME) + TEXT(";\n");
			OutHLSL += ReadFloatInBuffer(TEXT("temp_alpha"), TEXT("In_Row"), TEXT("In_TimeCol"));

			OutHLSL += TEXT("Out_Color.x = temp_color.x;\n");
			OutHLSL += TEXT("Out_Color.y = temp_color.y;\n");
			OutHLSL += TEXT("Out_Color.z = temp_color.z;\n");
			OutHLSL += TEXT("Out_Color.w = temp_alpha;\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == GetNumberOfPointsName)
	{
		// GetNumberOfPoints(out int Out_Value)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(out int Out_NumPoints) \n{\n");
			OutHLSL += TEXT("\tOut_NumPoints = ") + NumberOfPointsVar + TEXT(";\n");
		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == GetNumberOfRowsName)
	{
		// GetNumberOfRows(out int Out_Value)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(out int Out_NumRows) \n{\n");
			OutHLSL += TEXT("\tOut_NumRows = ") + NumberOfRowsVar + TEXT(";\n");
		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == GetNumberOfColumnsName)
	{
		// GetNumberOfColumns(out int Out_Value)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(out int Out_NumColumns) \n{\n");
			OutHLSL += TEXT("\tOut_NumColumns = ") + NumberOfColumnsVar + TEXT(";\n");
		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == GetLastRowIndexAtTimeName)
	{
		// GetLastRowIndexAtTime(float In_Time, out int Out_Value)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(float In_Time, out int Out_Value) \n{\n");

			OutHLSL += TEXT("\tint In_TimeCol = ") + GetSpecAttributeColumnIndex( EHoudiniAttributes::TIME ) + TEXT(";\n");
			OutHLSL += TEXT("\tfloat temp_time = 1.0;\n");
			OutHLSL += ReadFloatInBuffer(TEXT("temp_time"), NumberOfRowsVar + TEXT("- 1"), TEXT("In_TimeCol"));
			OutHLSL += TEXT("\tif ( temp_time < In_Time ) { Out_Value = ") + NumberOfRowsVar + TEXT(" - 1; return; }\n");
			OutHLSL += TEXT("\tint lastRow = -1;\n");
			OutHLSL += TEXT("\tfor( int n = 0; n < ") + NumberOfRowsVar + TEXT("; n++ )\n\t{\n");
				OutHLSL += TEXT("\t") + ReadFloatInBuffer(TEXT("temp_time"), TEXT("n"), TEXT("In_TimeCol"));
				OutHLSL += TEXT("\t\tif ( temp_time == In_Time ){ lastRow = n ;}");
				OutHLSL += TEXT("\t\telse if ( temp_time > In_Time ){ lastRow = n -1; return;}");
				OutHLSL += TEXT("\t\tif ( lastRow == -1 ){ lastRow = ") + NumberOfRowsVar + TEXT(" - 1; }");
			OutHLSL += TEXT("\t}\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == GetPointIDsToSpawnAtTimeName)
	{
		// GetPointIDsToSpawnAtTime(float In_Time, out int Out_MinID, out int Out_MaxID, out int Out_Count)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(float In_Time, out int Out_MinID, out int Out_MaxID, out int Out_Count) \n{\n");
			
			OutHLSL += TEXT("\tint time_col = ") + GetSpecAttributeColumnIndex(EHoudiniAttributes::TIME) + TEXT(";\n");
			OutHLSL += TEXT("\tif( time_col < 0 )\n");
				OutHLSL += TEXT("\t\t{ Out_Count = ") + NumberOfPointsVar +TEXT("; Out_MinID = 0; Out_MaxID = Out_Count - 1; return; }\n");

			// GetLastPointIDToSpawnAtTime
			OutHLSL += TEXT("\tint last_id = -1;\n");
			OutHLSL += TEXT("\tfloat temp_time = 0;\n");
			OutHLSL += TEXT("\tfor( int n = 0; n < ") + NumberOfPointsVar + TEXT("; n++ )\n\t{\n");
				OutHLSL += TEXT("\t\ttemp_time = ") + SpawnTimeBuffer + TEXT("[ n ];\n");
				OutHLSL += TEXT("\t\tif ( temp_time == In_Time )\n");
					OutHLSL += TEXT("\t\t\t{ last_id = n; }\n");
				OutHLSL += TEXT("\t\telse if ( temp_time > In_Time )\n");
					OutHLSL += TEXT("\t\t\t{ last_id = n - 1; break; }\n");
			OutHLSL += TEXT("\t}\n");

			// Check if we need to reset lastspawnedPointID
			OutHLSL += TEXT("\tif ( last_id < ") + LastSpawnedPointIDVar + TEXT(" || In_Time <= ") + LastSpawnTimeVar + TEXT(")\n");
				OutHLSL += TEXT("\t\t{") + LastSpawnedPointIDVar + TEXT(" = -1; }\n");

			// Nothing to spawn, In_Time is lower than the point's time
			OutHLSL += TEXT("\tif ( last_id < 0 )\n");
				OutHLSL += TEXT("\t\t{") + LastSpawnedPointIDVar + TEXT(" = -1;Out_Count = 0; Out_MinID = 0; Out_MaxID = 0;return;}\n");

			// We dont have any new point to spawn
			OutHLSL += TEXT("\tif ( last_id == ") + LastSpawnedPointIDVar + TEXT(")\n");
				OutHLSL += TEXT("{ Out_MinID = last_id; Out_MaxID = last_id; Out_Count = 0; return;}\n");

			OutHLSL += TEXT("\tOut_MinID = ") + LastSpawnedPointIDVar + TEXT(" + 1;\n");
			OutHLSL += TEXT("\tOut_MaxID = last_id;\n");
			OutHLSL += TEXT("\tOut_Count = Out_MaxID - Out_MinID + 1;\n");
			OutHLSL += TEXT("\t") + LastSpawnedPointIDVar + TEXT(" = Out_MaxID;\n");
			OutHLSL += TEXT("\t") + LastSpawnTimeVar + TEXT(" = In_Time;\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == GetRowIndexesForPointAtTimeName)
	{
		// GetRowIndexesForPointAtTime(int In_PointID, float In_Time, out int Out_PreviousRow, out int Out_NextRow, out float Out_Weight)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_PointID, float In_Time, out int Out_PreviousRow, out int Out_NextRow, out float Out_Weight) \n{\n");

			OutHLSL += TEXT("\tOut_PreviousRow = 0;\n\tOut_NextRow = 0;\n\tOut_Weight = 0;\n");
			OutHLSL += GetRowIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("Out_PreviousRow"), TEXT("Out_NextRow"), TEXT("Out_Weight"));

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == GetPointPositionAtTimeName)
	{
		// GetPointPositionAtTime(int In_PointID, float In_Time, out float3 Out_Value)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_PointID, float In_Time, out float3 Out_Value) \n{\n");
		OutHLSL += TEXT("Out_Value = float3( 0.0, 0.0, 0.0 );\n");
			OutHLSL += TEXT("\tint pos_col = ") + GetSpecAttributeColumnIndex(EHoudiniAttributes::POSITION) + TEXT(";\n");
			OutHLSL += TEXT("\tint prev_index = 0;int next_index = 0;float weight = 0.0f;\n");
			OutHLSL += GetRowIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));

			OutHLSL += TEXT("\tint In_DoSwap = 1;\n");
			OutHLSL += TEXT("\tint In_DoScale = 1;\n");

			OutHLSL += TEXT("\tfloat3 prev_vector = float3( 0.0, 0.0, 0.0 );\n");
			OutHLSL += ReadVectorInBuffer(TEXT("prev_vector"), TEXT("prev_index"), TEXT("pos_col"));
			OutHLSL += TEXT("\tfloat3 next_vector = float3( 0.0, 0.0, 0.0 );\n");
			OutHLSL += ReadVectorInBuffer(TEXT("next_vector"), TEXT("next_index"), TEXT("pos_col"));

			OutHLSL += TEXT("\tOut_Value = lerp(prev_vector, next_vector, weight);\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == GetPointValueAtTimeName)
	{
		// GetPointValueAtTime(int In_PointID, int In_Col, float In_Time, out float Out_Value)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_PointID, int In_Col, float In_Time, out float Out_Value) \n{\n");
		
		OutHLSL += TEXT("\tint prev_index = -1;int next_index = -1;float weight = 1.0f;\n");
		OutHLSL += GetRowIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));

		OutHLSL += TEXT("\tfloat prev_value;\n");
		OutHLSL += ReadFloatInBuffer(TEXT("prev_value"), TEXT("prev_index"), TEXT("In_Col"));
		OutHLSL += TEXT("\tfloat next_value;\n");
		OutHLSL += ReadFloatInBuffer(TEXT("next_value"), TEXT("next_index"), TEXT("In_Col"));

		OutHLSL += TEXT("\tOut_Value = lerp(prev_value, next_value, weight);\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == GetPointVectorValueAtTimeName)
	{
		// GetPointVectorValueAtTime(int In_PointID, int In_Col, float In_Time, out float3 Out_Value)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_PointID, int In_Col, float In_Time, out float3 Out_Value) \n{\n");

		OutHLSL += TEXT("\tint prev_index = -1;int next_index = -1;float weight = 1.0f;\n");
		OutHLSL += GetRowIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));

		OutHLSL += TEXT("\tint In_DoSwap = 1;\n");
		OutHLSL += TEXT("\tint In_DoScale = 1;\n");

		OutHLSL += TEXT("\tfloat3 prev_vector;\n");
		OutHLSL += ReadVectorInBuffer(TEXT("prev_vector"), TEXT("prev_index"), TEXT("In_Col"));
		OutHLSL += TEXT("\tfloat3 next_vector;\n");
		OutHLSL += ReadVectorInBuffer(TEXT("next_vector"), TEXT("next_index"), TEXT("In_Col"));

		OutHLSL += TEXT("\tOut_Value = lerp(prev_vector, next_vector, weight);\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == GetPointVectorValueAtTimeExName)
	{
		// GetPointVectorValueAtTimeEx(int In_PointID, int In_Col, float In_Time, int In_DoSwap, int In_DoScale, out float3 Out_Value)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_PointID, int In_Col, float In_Time, int In_DoSwap, int In_DoScale, out float3 Out_Value) \n{\n");

			OutHLSL += TEXT("\tint prev_index = -1;int next_index = -1;float weight = 1.0f;\n");
			OutHLSL += GetRowIndexesForPointAtTime(TEXT("In_PointID"), TEXT("In_Time"), TEXT("prev_index"), TEXT("next_index"), TEXT("weight"));

			OutHLSL += TEXT("\tfloat3 prev_vector;\n");
			OutHLSL += ReadVectorInBuffer(TEXT("prev_vector"), TEXT("prev_index"), TEXT("In_Col"));
			OutHLSL += TEXT("\tfloat3 next_vector;\n");
			OutHLSL += ReadVectorInBuffer(TEXT("next_vector"), TEXT("next_index"), TEXT("In_Col"));

			OutHLSL += TEXT("\tOut_Value = lerp(prev_vector, next_vector, weight);\n");

		OutHLSL += TEXT("\n}\n");

		return true;
	}
	else if (DefinitionFunctionName == GetPointLifeName)
	{
		// GetPointLife(int In_PointID, out float Out_Value)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_PointID, out float Out_Value) \n{\n");

			OutHLSL += TEXT("\tif ( ( In_PointID < 0 ) || ( In_PointID >= ") + NumberOfPointsVar + TEXT(") )\n");
				OutHLSL += TEXT("\t\t{Out_Value = -1.0f; return;}\n");
			OutHLSL += TEXT("\tOut_Value = ") + LifeValuesBuffer + TEXT("[ In_PointID ];\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	/*
	else if (DefinitionFunctionName == GetPointLifeAtTimeName)
	{
		// GetPointLifeAtTime(int In_PointID, float In_Time, out float Out_Value)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_PointID, float In_Time, out float Out_Value) \n{\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}
	*/
	else if (DefinitionFunctionName == GetPointTypeName)
	{
		// GetPointType(int In_PointID, out int Out_Value)
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(int In_PointID, out int Out_Value) \n{\n");

			OutHLSL += TEXT("\tif ( ( In_PointID < 0 ) || ( In_PointID >= ") + NumberOfPointsVar+ TEXT(") )\n");
				OutHLSL += TEXT("\t\t{Out_Value = -1; return;}\n");
			OutHLSL += TEXT("\tOut_Value = ") + PointTypesBuffer + TEXT("[ In_PointID ];\n");

		OutHLSL += TEXT("\n}\n");
		return true;
	}

	return !OutHLSL.IsEmpty();
}

// Build buffers and member variables HLSL definition
// Always use the BaseName + the DataInterfaceHLSL indentifier!
void UNiagaraDataInterfaceHoudiniCSV::GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	// int NumberOfRows_XX;
	FString BufferName = UNiagaraDataInterfaceHoudiniCSV::NumberOfRowsBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("int ") + BufferName + TEXT(";\n");

	// int NumberOfColumns_XX;
	BufferName = UNiagaraDataInterfaceHoudiniCSV::NumberOfColumnsBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("int ") + BufferName + TEXT(";\n");

	// int NumberOfPoints_XX;
	BufferName = UNiagaraDataInterfaceHoudiniCSV::NumberOfPointsBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("int ") + BufferName + TEXT(";\n");

	// Buffer<float> FloatValuesBuffer_XX;
	BufferName = UNiagaraDataInterfaceHoudiniCSV::FloatValuesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

	// Buffer<int> SpecialAttributesColumnIndexesBuffer_XX;
	BufferName = UNiagaraDataInterfaceHoudiniCSV::SpecialAttributesColumnIndexesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("Buffer<int> ") + BufferName + TEXT(";\n");

	// Buffer<float> SpawnTimesBuffer_XX;
	BufferName = UNiagaraDataInterfaceHoudiniCSV::SpawnTimesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

	// Buffer<float> LifeValuesBuffer_XX;
	BufferName = UNiagaraDataInterfaceHoudiniCSV::LifeValuesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

	// Buffer<int> PointTypesBuffer_XX;
	BufferName = UNiagaraDataInterfaceHoudiniCSV::PointTypesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("Buffer<int> ") + BufferName + TEXT(";\n");

	// int MaxNumberOfIndexesPerPoint_XX;
	BufferName = UNiagaraDataInterfaceHoudiniCSV::MaxNumberOfIndexesPerPointBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("int ") + BufferName + TEXT(";\n");

	// Buffer<int> PointValueIndexesBuffer_XX;
	BufferName = UNiagaraDataInterfaceHoudiniCSV::PointValueIndexesBufferBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("Buffer<int> ") + BufferName + TEXT(";\n");

	// int LastSpawnedPointId_XX;
	BufferName = UNiagaraDataInterfaceHoudiniCSV::LastSpawnedPointIdBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("int ") + BufferName + TEXT(";\n");

	// int LastSpawnTime_XX;
	BufferName = UNiagaraDataInterfaceHoudiniCSV::LastSpawnTimeBaseName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("int ") + BufferName + TEXT(";\n");
}

//FRWBuffer& UNiagaraDataInterfaceHoudiniCSV::GetFloatValuesGPUBuffer()
//{
//	//TODO: This isn't really very thread safe. Need to move to a proxy like system where DIs can push data to the RT safely.
//	if ( FloatValuesGPUBufferDirty )
//	{
//		uint32 NumElements = HoudiniCSVAsset ? HoudiniCSVAsset->FloatCSVData.Num() : 0;
//		if (NumElements <= 0)
//			return FloatValuesGPUBuffer;
//
//		FloatValuesGPUBuffer.Release();
//		FloatValuesGPUBuffer.Initialize( sizeof(float), NumElements, EPixelFormat::PF_R32_FLOAT, BUF_Static);
//
//		uint32 BufferSize = NumElements * sizeof( float );
//		float* BufferData = static_cast<float*>( RHILockVertexBuffer( FloatValuesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly ) );
//
//		if ( HoudiniCSVAsset )
//			FPlatformMemory::Memcpy( BufferData, HoudiniCSVAsset->FloatCSVData.GetData(), BufferSize );
//
//		RHIUnlockVertexBuffer( FloatValuesGPUBuffer.Buffer );
//		FloatValuesGPUBufferDirty = false;
//	}
//
//	return FloatValuesGPUBuffer;
//}

//FRWBuffer& UNiagaraDataInterfaceHoudiniCSV::GetSpecialAttributesColumnIndexesGPUBuffer()
//{
//	//TODO: This isn't really very thread safe. Need to move to a proxy like system where DIs can push data to the RT safely.
//	if ( SpecialAttributesColumnIndexesGPUBufferDirty )
//	{
//		uint32 NumElements = HoudiniCSVAsset ? HoudiniCSVAsset->SpecialAttributesColumnIndexes.Num() : 0;
//		if (NumElements <= 0)
//			return SpecialAttributesColumnIndexesGPUBuffer;
//
//		SpecialAttributesColumnIndexesGPUBuffer.Release();
//		SpecialAttributesColumnIndexesGPUBuffer.Initialize(sizeof(int32), NumElements, EPixelFormat::PF_R32_SINT, BUF_Static);
//
//		uint32 BufferSize = NumElements * sizeof(int32);
//		float* BufferData = static_cast<float*>(RHILockVertexBuffer(SpecialAttributesColumnIndexesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
//
//		if (HoudiniCSVAsset)
//			FPlatformMemory::Memcpy(BufferData, HoudiniCSVAsset->SpecialAttributesColumnIndexes.GetData(), BufferSize);
//
//		RHIUnlockVertexBuffer(SpecialAttributesColumnIndexesGPUBuffer.Buffer);
//		SpecialAttributesColumnIndexesGPUBufferDirty = false;
//	}
//
//	return SpecialAttributesColumnIndexesGPUBuffer;
//}

//FRWBuffer& UNiagaraDataInterfaceHoudiniCSV::GetSpawnTimesGPUBuffer()
//{
//	//TODO: This isn't really very thread safe. Need to move to a proxy like system where DIs can push data to the RT safely.
//	if ( SpawnTimesGPUBufferDirty )
//	{
//		uint32 NumElements = HoudiniCSVAsset ? HoudiniCSVAsset->SpawnTimes.Num() : 0;
//		if (NumElements <= 0)
//			return SpawnTimesGPUBuffer;
//
//		SpawnTimesGPUBuffer.Release();
//		SpawnTimesGPUBuffer.Initialize( sizeof(float), NumElements, EPixelFormat::PF_R32_FLOAT, BUF_Static);
//				
//		uint32 BufferSize = NumElements * sizeof( float );
//		float* BufferData = static_cast<float*>( RHILockVertexBuffer(SpawnTimesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly ) );
//
//		if ( HoudiniCSVAsset )
//			FPlatformMemory::Memcpy( BufferData, HoudiniCSVAsset->SpawnTimes.GetData(), BufferSize );
//
//		RHIUnlockVertexBuffer( SpawnTimesGPUBuffer.Buffer );
//		SpawnTimesGPUBufferDirty = false;
//	}
//
//	return SpawnTimesGPUBuffer;
//}

//FRWBuffer& UNiagaraDataInterfaceHoudiniCSV::GetLifeValuesGPUBuffer()
//{
//	//TODO: This isn't really very thread safe. Need to move to a proxy like system where DIs can push data to the RT safely.
//	if ( LifeValuesGPUBufferDirty )
//	{
//		uint32 NumElements = HoudiniCSVAsset ? HoudiniCSVAsset->LifeValues.Num() : 0;
//		if (NumElements <= 0)
//			return LifeValuesGPUBuffer;
//
//		LifeValuesGPUBuffer.Release();
//		LifeValuesGPUBuffer.Initialize( sizeof(float), NumElements, EPixelFormat::PF_R32_FLOAT, BUF_Static);
//
//		uint32 BufferSize = NumElements * sizeof( float );
//		float* BufferData = static_cast<float*>( RHILockVertexBuffer( LifeValuesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly ) );
//
//		if ( HoudiniCSVAsset )
//			FPlatformMemory::Memcpy( BufferData, HoudiniCSVAsset->LifeValues.GetData(), BufferSize );
//
//		RHIUnlockVertexBuffer(LifeValuesGPUBuffer.Buffer );
//		LifeValuesGPUBufferDirty = false;
//	}
//
//	return LifeValuesGPUBuffer;
//}

//FRWBuffer& UNiagaraDataInterfaceHoudiniCSV::GetPointTypesGPUBuffer()
//{
//	//TODO: This isn't really very thread safe. Need to move to a proxy like system where DIs can push data to the RT safely.
//	if ( PointTypesGPUBufferDirty )
//	{
//		uint32 NumElements = HoudiniCSVAsset ? HoudiniCSVAsset->PointTypes.Num() : 0;
//		if (NumElements <= 0)
//			return PointTypesGPUBuffer;
//
//		PointTypesGPUBuffer.Release();
//		PointTypesGPUBuffer.Initialize( sizeof(int32), NumElements, EPixelFormat::PF_R32_SINT, BUF_Static);
//
//		uint32 BufferSize = NumElements * sizeof(int32);
//		int32* BufferData = static_cast<int32*>( RHILockVertexBuffer(PointTypesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly ) );
//
//		if ( HoudiniCSVAsset )
//			FPlatformMemory::Memcpy( BufferData, HoudiniCSVAsset->PointTypes.GetData(), BufferSize );
//
//		RHIUnlockVertexBuffer(PointTypesGPUBuffer.Buffer );
//		PointTypesGPUBufferDirty = false;
//	}
//
//	return PointTypesGPUBuffer;
//}

//FRWBuffer& UNiagaraDataInterfaceHoudiniCSV::GetPointValueIndexesGPUBuffer()
//{
//	//TODO: This isn't really very thread safe. Need to move to a proxy like system where DIs can push data to the RT safely.
//	if ( PointValueIndexesGPUBufferDirty )
//	{
//		uint32 NumPoints = HoudiniCSVAsset ? HoudiniCSVAsset->PointValueIndexes.Num() : 0;
//		// Add an extra to the max number so all indexes end by a -1
//		uint32 MaxNumIndexesPerPoint = HoudiniCSVAsset ? HoudiniCSVAsset->GetMaxNumberOfPointValueIndexes() + 1: 0;
//		uint32 NumElements = NumPoints * MaxNumIndexesPerPoint;
//
//		if (NumElements <= 0)
//			return PointValueIndexesGPUBuffer;
//
//		TArray<int32> PointValueIndexes;
//		PointValueIndexes.Init(-1, NumElements);
//		if ( HoudiniCSVAsset )
//		{
//			// We need to flatten the nested array for HLSL conversion
//			for ( int32 PointID = 0; PointID < HoudiniCSVAsset->PointValueIndexes.Num(); PointID++ )
//			{
//				TArray<int32> RowIndexes = HoudiniCSVAsset->PointValueIndexes[PointID].RowIndexes;
//				for( int32 Idx = 0; Idx < RowIndexes.Num(); Idx++ )
//				{
//					PointValueIndexes[ PointID * MaxNumIndexesPerPoint + Idx ] = RowIndexes[ Idx ];
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

void UNiagaraDataInterfaceHoudiniCSV::PushToRenderThread()
{
	check(Proxy);
	FNiagaraDIHoudiniCSV_StaticDataPassToRT DataToPass;
	FMemory::Memzero(DataToPass);

	{
		uint32 NumElements = HoudiniCSVAsset ? HoudiniCSVAsset->FloatCSVData.Num() : 0;
		if (NumElements > 0)
		{
			DataToPass.FloatCSVData = new TArray<float>(HoudiniCSVAsset->FloatCSVData);
		}
	}

	{
		uint32 NumElements = HoudiniCSVAsset ? HoudiniCSVAsset->SpecialAttributesColumnIndexes.Num() : 0;
		if (NumElements > 0)
		{
			DataToPass.SpecialAttributesColumnIndexes = new TArray<int32>(HoudiniCSVAsset->SpecialAttributesColumnIndexes);
		}
	}

	{
		uint32 NumElements = HoudiniCSVAsset ? HoudiniCSVAsset->SpawnTimes.Num() : 0;
		if (NumElements > 0)
		{
			DataToPass.SpawnTimes = new TArray<float>(HoudiniCSVAsset->SpawnTimes);
		}
	}

	{
		uint32 NumElements = HoudiniCSVAsset ? HoudiniCSVAsset->LifeValues.Num() : 0;
		if (NumElements > 0)
		{
			DataToPass.LifeValues = new TArray<float>(HoudiniCSVAsset->LifeValues);
		}
	}

	{
		uint32 NumElements = HoudiniCSVAsset ? HoudiniCSVAsset->PointTypes.Num() : 0;
		if (NumElements > 0)
		{
			DataToPass.PointTypes = new TArray<int32>(HoudiniCSVAsset->PointTypes);
		}
	}

	{
		uint32 NumPoints = HoudiniCSVAsset ? HoudiniCSVAsset->PointValueIndexes.Num() : 0;
		// Add an extra to the max number so all indexes end by a -1
		uint32 MaxNumIndexesPerPoint = HoudiniCSVAsset ? HoudiniCSVAsset->GetMaxNumberOfPointValueIndexes() + 1 : 0;
		uint32 NumElements = NumPoints * MaxNumIndexesPerPoint;

		if (NumElements > 0)
		{
			DataToPass.PointValueIndexes = new TArray<int32>;
			DataToPass.PointValueIndexes->Init(-1, NumElements);
			if (HoudiniCSVAsset)
			{
				// We need to flatten the nested array for HLSL conversion
				for (int32 PointID = 0; PointID < HoudiniCSVAsset->PointValueIndexes.Num(); PointID++)
				{
					TArray<int32> RowIndexes = HoudiniCSVAsset->PointValueIndexes[PointID].RowIndexes;
					for (int32 Idx = 0; Idx < RowIndexes.Num(); Idx++)
					{
						(*DataToPass.PointValueIndexes)[PointID * MaxNumIndexesPerPoint + Idx] = RowIndexes[Idx];
					}
				}
			}
		}
	}

	DataToPass.NumRows = GetNumberOfRows();
	DataToPass.NumColumns = GetNumberOfColumns();
	DataToPass.NumPoints = GetNumberOfPoints();
	DataToPass.MaxNumIndexesPerPoint = GetMaxNumberOfIndexesPerPoints();

	FNiagaraDataInterfaceProxyHoudiniCSV* ThisProxy = GetProxyAs<FNiagaraDataInterfaceProxyHoudiniCSV>();
	ENQUEUE_RENDER_COMMAND(FNiagaraDIHoudiniCSV_ToRT) (
		[ThisProxy, DataToPass](FRHICommandListImmediate& CmdList) mutable
	{
		ThisProxy->AcceptStaticDataUpdate(DataToPass);
	}
	);
}

void FNiagaraDataInterfaceProxyHoudiniCSV::AcceptStaticDataUpdate(FNiagaraDIHoudiniCSV_StaticDataPassToRT& Update)
{
	if (Update.FloatCSVData)
	{
		int32 NumElements = Update.FloatCSVData->Num();
		FloatValuesGPUBuffer.Release();
		FloatValuesGPUBuffer.Initialize(sizeof(float), NumElements, EPixelFormat::PF_R32_FLOAT, BUF_Static);

		uint32 BufferSize = NumElements * sizeof(float);
		float* BufferData = static_cast<float*>(RHILockVertexBuffer(FloatValuesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));

		FPlatformMemory::Memcpy(BufferData, Update.FloatCSVData->GetData(), BufferSize);

		RHIUnlockVertexBuffer(FloatValuesGPUBuffer.Buffer);

		delete Update.FloatCSVData;
	}

	if (Update.SpecialAttributesColumnIndexes)
	{
		uint32 NumElements = Update.SpecialAttributesColumnIndexes->Num();

		SpecialAttributesColumnIndexesGPUBuffer.Release();
		SpecialAttributesColumnIndexesGPUBuffer.Initialize(sizeof(int32), NumElements, EPixelFormat::PF_R32_SINT, BUF_Static);

		uint32 BufferSize = NumElements * sizeof(int32);
		float* BufferData = static_cast<float*>(RHILockVertexBuffer(SpecialAttributesColumnIndexesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));

		FPlatformMemory::Memcpy(BufferData, Update.SpecialAttributesColumnIndexes->GetData(), BufferSize);

		RHIUnlockVertexBuffer(SpecialAttributesColumnIndexesGPUBuffer.Buffer);

		delete Update.SpecialAttributesColumnIndexes;
	}

	if (Update.SpawnTimes)
	{
		uint32 NumElements = Update.SpawnTimes->Num();
		

		SpawnTimesGPUBuffer.Release();
		SpawnTimesGPUBuffer.Initialize(sizeof(float), NumElements, EPixelFormat::PF_R32_FLOAT, BUF_Static);

		uint32 BufferSize = NumElements * sizeof(float);
		float* BufferData = static_cast<float*>(RHILockVertexBuffer(SpawnTimesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));

		FPlatformMemory::Memcpy(BufferData, Update.SpawnTimes->GetData(), BufferSize);

		RHIUnlockVertexBuffer(SpawnTimesGPUBuffer.Buffer);

		delete Update.SpawnTimes;		
	}

	if (Update.LifeValues)
	{
		uint32 NumElements = Update.LifeValues->Num();

		LifeValuesGPUBuffer.Release();
		LifeValuesGPUBuffer.Initialize(sizeof(float), NumElements, EPixelFormat::PF_R32_FLOAT, BUF_Static);

		uint32 BufferSize = NumElements * sizeof(float);
		float* BufferData = static_cast<float*>(RHILockVertexBuffer(LifeValuesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));

		FPlatformMemory::Memcpy(BufferData, Update.LifeValues->GetData(), BufferSize);

		RHIUnlockVertexBuffer(LifeValuesGPUBuffer.Buffer);
		
		delete Update.LifeValues;
	}

	if (Update.PointTypes)
	{
		uint32 NumElements = Update.PointTypes->Num();

		PointTypesGPUBuffer.Release();
		PointTypesGPUBuffer.Initialize(sizeof(int32), NumElements, EPixelFormat::PF_R32_SINT, BUF_Static);

		uint32 BufferSize = NumElements * sizeof(int32);
		int32* BufferData = static_cast<int32*>(RHILockVertexBuffer(PointTypesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));

		FPlatformMemory::Memcpy(BufferData, Update.PointTypes->GetData(), BufferSize);

		RHIUnlockVertexBuffer(PointTypesGPUBuffer.Buffer);
		
		delete Update.PointTypes;
	}

	if (Update.PointValueIndexes)
	{
		
		uint32 NumElements = Update.PointValueIndexes->Num();

		PointValueIndexesGPUBuffer.Release();
		PointValueIndexesGPUBuffer.Initialize(sizeof(int32), NumElements, EPixelFormat::PF_R32_SINT, BUF_Static);

		uint32 BufferSize = NumElements * sizeof(int32);
		int32* BufferData = static_cast<int32*>(RHILockVertexBuffer(PointValueIndexesGPUBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
		FPlatformMemory::Memcpy(BufferData, Update.PointValueIndexes->GetData(), BufferSize);
		RHIUnlockVertexBuffer(PointValueIndexesGPUBuffer.Buffer);

		delete Update.PointValueIndexes;
	}

	NumRows = Update.NumRows;
	NumColumns = Update.NumColumns;
	NumPoints = Update.NumPoints;
	MaxNumberOfIndexesPerPoint = Update.MaxNumIndexesPerPoint;
}

// Parameters used for GPU sim compatibility
struct FNiagaraDataInterfaceParametersCS_HoudiniCSV : public FNiagaraDataInterfaceParametersCS
{
	virtual void Bind(const FNiagaraDataInterfaceParamRef& ParamRef, const class FShaderParameterMap& ParameterMap) override
	{
		NumberOfRows.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudiniCSV::NumberOfRowsBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));
		NumberOfColumns.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudiniCSV::NumberOfColumnsBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));
		NumberOfPoints.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudiniCSV::NumberOfPointsBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));
		
		FloatValuesBuffer.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudiniCSV::FloatValuesBufferBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));

		SpecialAttributesColumnIndexesBuffer.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudiniCSV::SpecialAttributesColumnIndexesBufferBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));		

		SpawnTimesBuffer.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudiniCSV::SpawnTimesBufferBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));
		LifeValuesBuffer.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudiniCSV::LifeValuesBufferBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));
		PointTypesBuffer.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudiniCSV::PointTypesBufferBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));

		MaxNumberOfIndexesPerPoint.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudiniCSV::MaxNumberOfIndexesPerPointBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));
		PointValueIndexesBuffer.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudiniCSV::PointValueIndexesBufferBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));

		LastSpawnedPointId.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudiniCSV::LastSpawnedPointIdBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));
		LastSpawnTime.Bind(ParameterMap, *(UNiagaraDataInterfaceHoudiniCSV::LastSpawnTimeBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));
	}

	virtual void Serialize(FArchive& Ar)override
	{
		Ar << Version;

		Ar << NumberOfRows;
		Ar << NumberOfColumns;
		Ar << NumberOfPoints;

		Ar << FloatValuesBuffer;
		Ar << SpecialAttributesColumnIndexesBuffer;

		Ar << SpawnTimesBuffer;
		Ar << LifeValuesBuffer;
		Ar << PointTypesBuffer;

		Ar << MaxNumberOfIndexesPerPoint;
		Ar << PointValueIndexesBuffer;

		Ar << LastSpawnedPointId;
		Ar << LastSpawnTime;
	}

	virtual void Set(FRHICommandList& RHICmdList, const FNiagaraDataInterfaceSetArgs& Context) const override
	{
		check( IsInRenderingThread() );

		FRHIComputeShader* ComputeShaderRHI = Context.Shader->GetComputeShader();
		FNiagaraDataInterfaceProxyHoudiniCSV* HoudiniDI = static_cast<FNiagaraDataInterfaceProxyHoudiniCSV*>(Context.DataInterface);
		if ( !HoudiniDI )
		{
			return;
		}

		SetShaderValue(RHICmdList, ComputeShaderRHI, NumberOfRows, HoudiniDI->NumRows);
		SetShaderValue(RHICmdList, ComputeShaderRHI, NumberOfColumns, HoudiniDI->NumColumns);
		SetShaderValue(RHICmdList, ComputeShaderRHI, NumberOfPoints, HoudiniDI->NumColumns);

		FRWBuffer& FloatRWBuffer = HoudiniDI->FloatValuesGPUBuffer;
 		RHICmdList.SetShaderResourceViewParameter( ComputeShaderRHI, FloatValuesBuffer.GetBaseIndex(), FloatRWBuffer.SRV );

		FRWBuffer& AttributeColumnRWBuffer = HoudiniDI->SpecialAttributesColumnIndexesGPUBuffer;
		RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SpecialAttributesColumnIndexesBuffer.GetBaseIndex(), AttributeColumnRWBuffer.SRV);

		FRWBuffer& SpawnRWBuffer = HoudiniDI->SpawnTimesGPUBuffer;
		RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SpawnTimesBuffer.GetBaseIndex(), SpawnRWBuffer.SRV);
		FRWBuffer& LifeRWBuffer = HoudiniDI->LifeValuesGPUBuffer;
		RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, LifeValuesBuffer.GetBaseIndex(), LifeRWBuffer.SRV);
		FRWBuffer& TypesRWBuffer = HoudiniDI->PointTypesGPUBuffer;
		RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, PointTypesBuffer.GetBaseIndex(), TypesRWBuffer.SRV);

		SetShaderValue(RHICmdList, ComputeShaderRHI, MaxNumberOfIndexesPerPoint, HoudiniDI->MaxNumberOfIndexesPerPoint);

		FRWBuffer& PointValuesIndexesRWBuffer = HoudiniDI->PointValueIndexesGPUBuffer;
		RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, PointValueIndexesBuffer.GetBaseIndex(), PointValuesIndexesRWBuffer.SRV);

		SetShaderValue(RHICmdList, ComputeShaderRHI, LastSpawnedPointId, -1);
		SetShaderValue(RHICmdList, ComputeShaderRHI, LastSpawnTime, -1);
	}

private:

	FShaderParameter NumberOfRows;
	FShaderParameter NumberOfColumns;
	FShaderParameter NumberOfPoints;

	FShaderResourceParameter FloatValuesBuffer;
	FShaderResourceParameter SpecialAttributesColumnIndexesBuffer;

	FShaderResourceParameter SpawnTimesBuffer;
	FShaderResourceParameter LifeValuesBuffer;
	FShaderResourceParameter PointTypesBuffer;

	FShaderParameter MaxNumberOfIndexesPerPoint;
	FShaderResourceParameter PointValueIndexesBuffer;

	FShaderParameter LastSpawnedPointId;
	FShaderParameter LastSpawnTime;

	uint32 Version = 1;
};

FNiagaraDataInterfaceParametersCS* UNiagaraDataInterfaceHoudiniCSV::ConstructComputeParameters()const
{
	return new FNiagaraDataInterfaceParametersCS_HoudiniCSV();
}

#undef LOCTEXT_NAMESPACE
