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

#include "HoudiniCSVFactory.h"
#include "HoudiniCSV.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY(LogHoudiniNiagaraEditor);

#define LOCTEXT_NAMESPACE "HoudiniCSVFactory"
 
/////////////////////////////////////////////////////
// UHoudiniCSVFactory 
UHoudiniCSVFactory::UHoudiniCSVFactory(const FObjectInitializer& ObjectInitializer) : Super( ObjectInitializer )
{
	// This factory is responsible for manufacturing HoudiniEngine assets.
	SupportedClass = UHoudiniCSV::StaticClass();

	// This factory does not manufacture new objects from scratch.
	bCreateNew = false;

	// This factory will open the editor for each new object.
	bEditAfterNew = false;

	// This factory will import objects from files.
	bEditorImport = true;

	// Factory does not import objects from text.
	bText = true;

	// Add supported formats.
	Formats.Add(FString(TEXT("hcsv;")) + NSLOCTEXT("HoudiniCSVFactory", "FormatHCSV", "HCSV File").ToString());
}


UObject* 
UHoudiniCSVFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UHoudiniCSV* NewHoudiniCSVObject = NewObject<UHoudiniCSV>(InParent, Class, Name, Flags | RF_Transactional);
	return NewHoudiniCSVObject;
}

UObject* 
UHoudiniCSVFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;

	UHoudiniCSV* NewHoudiniCSVObject = NewObject<UHoudiniCSV>(InParent, InClass, InName, Flags);
	if ( !NewHoudiniCSVObject )
		return nullptr;

	if ( !NewHoudiniCSVObject->UpdateFromFile( Filename ) )
		return nullptr;   

	// Create reimport information.
	UAssetImportData * AssetImportData = NewHoudiniCSVObject->AssetImportData;
	if (!AssetImportData)
	{
		AssetImportData = NewObject< UAssetImportData >(NewHoudiniCSVObject, UAssetImportData::StaticClass());
		NewHoudiniCSVObject->AssetImportData = AssetImportData;
	}

	//NewHoudiniCSVObject->AssetImportData->Update(Filename);
	AssetImportData->Update(UFactory::GetCurrentFilename());

	// Broadcast notification that the new asset has been imported.
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, NewHoudiniCSVObject);

	return NewHoudiniCSVObject;
}

FText
UHoudiniCSVFactory::GetDisplayName() const
{
	return LOCTEXT("HoudiniCSVFactoryDescription", "Houdini CSV File");
}

bool
UHoudiniCSVFactory::DoesSupportClass(UClass * Class)
{
	return Class == SupportedClass;
}
bool 
UHoudiniCSVFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension(Filename);

	if (Extension == TEXT("hcsv") )
	{
		return true;
	}
	return false;
}

bool 
UHoudiniCSVFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
    UHoudiniCSV* HoudiniCSV = Cast<UHoudiniCSV>(Obj);
    if (HoudiniCSV)
    {
		if (HoudiniCSV->AssetImportData)
		{
			HoudiniCSV->AssetImportData->ExtractFilenames(OutFilenames);
		}
		else
		{
			OutFilenames.Add(TEXT(""));
		}
		return true;
    }
    return false;
}

void 
UHoudiniCSVFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UHoudiniCSV* HoudiniCSV = Cast<UHoudiniCSV>( Obj );
	if ( HoudiniCSV 
		&& HoudiniCSV->AssetImportData
		&&  ensure( NewReimportPaths.Num() == 1 ) )
	{
		HoudiniCSV->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type 
UHoudiniCSVFactory::Reimport(UObject* Obj)
{
	UHoudiniCSV* HoudiniCSV = Cast<UHoudiniCSV>(Obj);
	if (!HoudiniCSV || !HoudiniCSV->AssetImportData )
		return EReimportResult::Failed;

	const FString FilePath = HoudiniCSV->AssetImportData->GetFirstFilename();
	if (!FilePath.Len())
		return EReimportResult::Failed;

	UE_LOG(LogHoudiniNiagaraEditor, Log, TEXT("Performing atomic reimport of [%s]"), *FilePath);

	// Ensure that the file provided by the path exists
	if (IFileManager::Get().FileSize(*FilePath) == INDEX_NONE)
	{
		UE_LOG(LogHoudiniNiagaraEditor, Warning, TEXT("Cannot reimport: source file cannot be found."));
		return EReimportResult::Failed;
	}

	bool OutCanceled = false;
	if (ImportObject(HoudiniCSV->GetClass(), HoudiniCSV->GetOuter(), *HoudiniCSV->GetName(), RF_Public | RF_Standalone, FilePath, nullptr, OutCanceled) != nullptr)
	{
		UE_LOG(LogHoudiniNiagaraEditor, Log, TEXT("Imported successfully"));
		// Try to find the outer package so we can dirty it up
		if (HoudiniCSV->GetOuter())
			HoudiniCSV->GetOuter()->MarkPackageDirty();
		else
			HoudiniCSV->MarkPackageDirty();
	}
	else if (OutCanceled)
	{
		UE_LOG(LogHoudiniNiagaraEditor, Warning, TEXT("-- import canceled"));
	}
	{
		UE_LOG(LogHoudiniNiagaraEditor, Warning, TEXT("-- import failed"));
	}

	return EReimportResult::Succeeded;
}

int32 
UHoudiniCSVFactory::GetPriority() const
{
	return ImportPriority;
}

#undef LOCTEXT_NAMESPACE