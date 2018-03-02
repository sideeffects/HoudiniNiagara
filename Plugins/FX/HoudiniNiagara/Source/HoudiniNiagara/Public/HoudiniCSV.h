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

#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "HoudiniCSV.generated.h"

UCLASS()
class HOUDININIAGARA_API UHoudiniCSV : public UObject
{
    GENERATED_UCLASS_BODY()
 
    public:
	
	//-----------------------------------------------------------------------------------------
	//  MEMBER FUNCTIONS
	//-----------------------------------------------------------------------------------------

	bool UpdateFromFile( const FString& TheFileName );
	bool UpdateFromStringArray( TArray<FString>& StringArray );

	void SetFileName( const FString& TheFilename );

	// Returns the number of points found in the CSV file
	int32 GetNumberOfPointsInCSV();

	// Returns the column index for a given string
	bool GetColumnIndex(const FString& ColumnTitle, int32& ColumnIndex);

	// Returns the float value at a given point in the CSV file
	bool GetCSVFloatValue( const int32& lineIndex, const int32& colIndex, float& value );
	// Returns the float value at a given point in the CSV file
	bool GetCSVFloatValue(const int32& lineIndex, const FString& ColumnTitle, float& value);
	// Returns the float value at a given point in the CSV file
	bool GetCSVStringValue( const int32& lineIndex, const int32& colIndex, FString& value );
	// Returns the string value at a given point in the CSV file
	bool GetCSVStringValue( const int32& lineIndex, const FString& ColumnTitle, FString& value );
	// Returns a Vector3 for a given point in the CSV file
	bool GetCSVVectorValue( const int32& lineIndex, const int32& colIndex, FVector& value, const bool& DoSwap = true, const bool& DoScale = true );
	
	// Returns a time value for a given point in the CSV file
	bool GetCSVTimeValue( const int32& lineIndex, float& value );
	// Returns a Position Vector3 for a given point in the CSV file (converted to unreal's coordinate system)
	bool GetCSVPositionValue( const int32& lineIndex, FVector& value );
	// Returns a Normal Vector3 for a given point in the CSV file (converted to unreal's coordinate system)
	bool GetCSVNormalValue( const int32& lineIndex, FVector& value );

	// Returns the last index of the particles that should be spawned at time t
	bool GetLastParticleIndexAtTime( const float& time, int32& lastIndex );

	//-----------------------------------------------------------------------------------------
	//  MEMBER VARIABLES
	//-----------------------------------------------------------------------------------------
	//UPROPERTY(EditAnywhere, Category = "My Object Properties")
	UPROPERTY( VisibleAnywhere, Category = "Houdini CSV File Properties" )
	FString FileName;

	// The number of values stored in the CSV file (excluding the title row)
	UPROPERTY( VisibleAnywhere, Category = "Houdini CSV File Properties" )
	int32 NumberOfLines;

	// The number of value TYPES stored in the CSV file
	UPROPERTY( VisibleAnywhere, Category = "Houdini CSV File Properties" )
	int32 NumberOfColumns;

	// The tokenized title raw, describing the content of each column
	UPROPERTY( VisibleAnywhere, Category = "Houdini CSV File Properties" )
	TArray<FString> TitleRowArray;

    private:

	// Array containing the Raw String data
	UPROPERTY()
	TArray<FString> StringCSVData;

	// Array containing all the CSV data converted to floats
	UPROPERTY()
	TArray<float> FloatCSVData;

	// Array containing the Position buffer
	UPROPERTY()	    
	int32 PositionColumnIndex;
	//TArray<float> PositionData;

	// Array containing the Normal buffer
	UPROPERTY()
	int32 NormalColumnIndex;
	//TArray<float> NormalData;

	// Array containing the time buffer
	UPROPERTY()
	int32 TimeColumnIndex;
	//TArray<float> TimeData;
};