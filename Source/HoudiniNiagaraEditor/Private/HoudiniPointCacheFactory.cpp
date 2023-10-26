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

#include "HoudiniPointCacheFactory.h"
#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "Misc/CoreMiscDefines.h" 
#include "HoudiniPointCache.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY(LogHoudiniNiagaraEditor);

#define LOCTEXT_NAMESPACE "HoudiniPointCacheFactory"
 
/////////////////////////////////////////////////////
// UHoudiniPointCacheFactory 
UHoudiniPointCacheFactory::UHoudiniPointCacheFactory(const FObjectInitializer& ObjectInitializer) : Super( ObjectInitializer )
{
	// This factory is responsible for manufacturing HoudiniEngine assets.
	SupportedClass = UHoudiniPointCache::StaticClass();

	// This factory does not manufacture new objects from scratch.
	bCreateNew = false;

	// This factory will open the editor for each new object.
	bEditAfterNew = false;

	// This factory will import objects from files.
	bEditorImport = true;

	// Factory does not import objects from text.
	bText = true;

	// Add supported formats.
	Formats.Add(FString(TEXT("hcsv;")) + NSLOCTEXT("HoudiniPointCacheFactory", "FormatHCSV", "HCSV File").ToString());
	Formats.Add(FString(TEXT("hjson;")) + NSLOCTEXT("HoudiniPointCacheFactory", "FormatHJSON", "HJSON File").ToString());
	Formats.Add(FString(TEXT("hbjson;")) + NSLOCTEXT("HoudiniPointCacheFactory", "FormatHBJSON", "HBJSON File").ToString());
}


UObject* 
UHoudiniPointCacheFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UHoudiniPointCache* NewHoudiniPointCacheObject = NewObject<UHoudiniPointCache>(InParent, Class, Name, Flags | RF_Transactional);
	return NewHoudiniPointCacheObject;
}

UObject* 
UHoudiniPointCacheFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;

	UHoudiniPointCache* NewHoudiniPointCacheObject = NewObject<UHoudiniPointCache>(InParent, InClass, InName, Flags);
	if ( !NewHoudiniPointCacheObject )
		return nullptr;

	if ( !NewHoudiniPointCacheObject->UpdateFromFile( Filename ) )
		return nullptr;   

	// Create reimport information.
	UAssetImportData * AssetImportData = NewHoudiniPointCacheObject->AssetImportData;
	if (!AssetImportData)
	{
		AssetImportData = NewObject< UAssetImportData >(NewHoudiniPointCacheObject, UAssetImportData::StaticClass());
		NewHoudiniPointCacheObject->AssetImportData = AssetImportData;
	}

	//NewHoudiniPointCacheObject->AssetImportData->Update(Filename);
	AssetImportData->Update(UFactory::GetCurrentFilename());

	// Broadcast notification that the new asset has been imported.
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, NewHoudiniPointCacheObject);

	return NewHoudiniPointCacheObject;
}

FText
UHoudiniPointCacheFactory::GetDisplayName() const
{
	return LOCTEXT("HoudiniPointCacheFactoryDescription", "Houdini Point Cache File");
}

bool
UHoudiniPointCacheFactory::DoesSupportClass(UClass * Class)
{
	return Class == SupportedClass;
}
bool 
UHoudiniPointCacheFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension(Filename).ToLower();

	if (Extension == TEXT("hcsv") || Extension == TEXT("hbjson") || Extension == TEXT("hjson"))
	{
		return true;
	}
	return false;
}

bool 
UHoudiniPointCacheFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
    UHoudiniPointCache* HoudiniPointCache = Cast<UHoudiniPointCache>(Obj);
    if (HoudiniPointCache)
    {
		if (HoudiniPointCache->AssetImportData)
		{
			HoudiniPointCache->AssetImportData->ExtractFilenames(OutFilenames);
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
UHoudiniPointCacheFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UHoudiniPointCache* HoudiniPointCache = Cast<UHoudiniPointCache>( Obj );
	if ( HoudiniPointCache 
		&& HoudiniPointCache->AssetImportData
		&&  ensure( NewReimportPaths.Num() == 1 ) )
	{
		HoudiniPointCache->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type 
UHoudiniPointCacheFactory::Reimport(UObject* Obj)
{
	UHoudiniPointCache* HoudiniPointCache = Cast<UHoudiniPointCache>(Obj);
	if (!HoudiniPointCache || !HoudiniPointCache->AssetImportData )
		return EReimportResult::Failed;

	const FString FilePath = HoudiniPointCache->AssetImportData->GetFirstFilename();
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
	if (ImportObject(HoudiniPointCache->GetClass(), HoudiniPointCache->GetOuter(), *HoudiniPointCache->GetName(), RF_Public | RF_Standalone, FilePath, nullptr, OutCanceled) != nullptr)
	{
		UE_LOG(LogHoudiniNiagaraEditor, Log, TEXT("Imported successfully"));
		// Try to find the outer package so we can dirty it up
		if (HoudiniPointCache->GetOuter())
			HoudiniPointCache->GetOuter()->MarkPackageDirty();
		else
			HoudiniPointCache->MarkPackageDirty();
	}
	else if (OutCanceled)
	{
		UE_LOG(LogHoudiniNiagaraEditor, Warning, TEXT("-- import canceled"));
	}
	else
	{
		UE_LOG(LogHoudiniNiagaraEditor, Warning, TEXT("-- import failed"));
	}

	return EReimportResult::Succeeded;
}

int32 
UHoudiniPointCacheFactory::GetPriority() const
{
	return ImportPriority;
}

#undef LOCTEXT_NAMESPACE