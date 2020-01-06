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
#include "HoudiniCSVAssetActions.h"

#include "ToolMenus.h"
#include "HoudiniCSV.h"
#include "EditorStyleSet.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "EditorReimportHandler.h"
#include "HAL/FileManager.h"

//#include "HoudiniCSVAssetEditorToolkit.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"


FHoudiniCSVAssetActions::FHoudiniCSVAssetActions()
{ }

bool FHoudiniCSVAssetActions::CanFilter()
{
    return true;
}


void FHoudiniCSVAssetActions::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
    FAssetTypeActions_Base::GetActions(InObjects, Section);

    auto HoudiniCSVAssets = GetTypedWeakObjectPtrs<UHoudiniCSV>(InObjects);

    Section.AddMenuEntry(
		"ReimportHoudiniCSVLabel",
		LOCTEXT("ReimportHoudiniCSVLabel", "Reimport"),
		LOCTEXT("ReimportHoudiniCSVTooltip", "Reimport the selected Houdini CSV file(s)."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.ReimportAsset"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FHoudiniCSVAssetActions::ExecuteReimport, HoudiniCSVAssets),
			FCanExecuteAction::CreateSP(this, &FHoudiniCSVAssetActions::CanExecuteReimport, HoudiniCSVAssets)
		)
    );

    Section.AddMenuEntry(
		"OpenHoudiniCSVLabel",
		LOCTEXT("OpenHoudiniCSVLabel", "Open in Text Editor"),
		LOCTEXT("OpenHoudiniCSVTooltip", "Open the selected Houdini CSV file(s) in a Text Editor."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.OpenInExternalEditor"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FHoudiniCSVAssetActions::ExecuteOpenInEditor, HoudiniCSVAssets),
			FCanExecuteAction::CreateSP(this, &FHoudiniCSVAssetActions::CanExecuteOpenInEditor, HoudiniCSVAssets)
		)
    );

	Section.AddMenuEntry(
		"FindHoudiniCSVInExplorer",
		LOCTEXT("FindHoudiniCSVInExplorer", "Find Source CSV"),
		LOCTEXT("FindHoudiniCSVInExplorerTooltip", "Opens explorer at the location of this asset's source CSV file."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.OpenInExternalEditor"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FHoudiniCSVAssetActions::ExecuteFindInExplorer, HoudiniCSVAssets),
			FCanExecuteAction::CreateSP(this, &FHoudiniCSVAssetActions::CanExecuteFindInExplorer, HoudiniCSVAssets)
		)
	);
}


uint32 FHoudiniCSVAssetActions::GetCategories()
{
    return EAssetTypeCategories::Misc;
}


FText FHoudiniCSVAssetActions::GetName() const
{
    return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_HoudiniCSV", "Houdini CSV Asset");
}


UClass* FHoudiniCSVAssetActions::GetSupportedClass() const
{
    return UHoudiniCSV::StaticClass();
}

FText FHoudiniCSVAssetActions::GetAssetDescription(const FAssetData& AssetData) const
{
	/*
	UHoudiniCSV* CSVAsset = Cast<UHoudiniCSV>(AssetData.GetAsset());
	if ( !CSVAsset )
		return FText::GetEmpty();

	FString StrDescription;
	StrDescription += TEXT("Number of Rows: ") + FString::FromInt(CSVAsset->NumberOfRows) + TEXT("\n");
	StrDescription += TEXT("Number of Columns: ") + FString::FromInt(CSVAsset->NumberOfColumns) + TEXT("\n");
	StrDescription += TEXT("Number of Points: ") + FString::FromInt(CSVAsset->NumberOfPoints) + TEXT("\n");

	return FText::FromString(StrDescription);
	*/
	return FText::GetEmpty();
}


FColor FHoudiniCSVAssetActions::GetTypeColor() const
{
    //return FColor::Orange;
	return FColor(255, 165, 0);
}


bool FHoudiniCSVAssetActions::HasActions(const TArray<UObject*>& InObjects) const
{
    return true;
}


class UThumbnailInfo* FHoudiniCSVAssetActions::GetThumbnailInfo(UObject* Asset) const
{
    /*
    UHoudiniCSV* HoudiniCSV = CastChecked<UHoudiniCSV>(Asset);
    UThumbnailInfo* ThumbnailInfo = HoudiniCSV->ThumbnailInfo;
    if (ThumbnailInfo == NULL)
    {
	ThumbnailInfo = NewObject<USceneThumbnailInfo>(HoudiniCSV, NAME_None, RF_Transactional);
	HoudiniCSV->ThumbnailInfo = ThumbnailInfo;
    }

    return ThumbnailInfo;
    */

    return nullptr;
}


bool FHoudiniCSVAssetActions::CanExecuteReimport(const TArray<TWeakObjectPtr<UHoudiniCSV>> Objects) const
{
    for ( auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt )
    {
		auto Object = (*ObjIt).Get();
		if (!Object)
			continue;

		// If one object is valid, we can execute the action
		return true;
    }

    return false;
}

void FHoudiniCSVAssetActions::ExecuteReimport(const TArray<TWeakObjectPtr<UHoudiniCSV>> Objects) const
{
    for ( auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt )
    {
		auto Object = (*ObjIt).Get();
		if (!Object)
			continue;

		FReimportManager::Instance()->Reimport(Object, true);
    }
}


bool 
FHoudiniCSVAssetActions::CanExecuteOpenInEditor(const TArray<TWeakObjectPtr<UHoudiniCSV>> Objects) const
{
    for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
    {
		auto Object = ( *ObjIt ).Get();
		if (!Object)
			continue;

		const FString SourceFilePath = Object->FileName;
		if (!SourceFilePath.Len() || IFileManager::Get().FileSize(*SourceFilePath) == INDEX_NONE)
			continue;

		// If one object is valid, we can execute the action
		return true;
    }

    return false;
}

void
FHoudiniCSVAssetActions::ExecuteOpenInEditor(const TArray<TWeakObjectPtr<UHoudiniCSV>> Objects) const
{
    for ( auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt )
    {
		auto Object = ( *ObjIt ).Get();
		if (!Object)
			continue;

		const FString SourceFilePath = Object->FileName;
		if (!SourceFilePath.Len() || IFileManager::Get().FileSize(*SourceFilePath) == INDEX_NONE)
			continue;

		FPlatformProcess::LaunchFileInDefaultExternalApplication(*SourceFilePath, NULL, ELaunchVerb::Open);
    }
}

bool
FHoudiniCSVAssetActions::CanExecuteFindInExplorer(const TArray<TWeakObjectPtr<UHoudiniCSV>> Objects) const
{
	for ( auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt )
	{
		auto Object = (*ObjIt).Get();
		if (!Object)
			continue;

		const FString SourceFilePath = Object->FileName;
		if (!SourceFilePath.Len() || IFileManager::Get().FileSize(*SourceFilePath) == INDEX_NONE)
			continue;

		// If one object is valid, we can execute the action
		return true;
	}

	return false;
}

void
FHoudiniCSVAssetActions::ExecuteFindInExplorer(const TArray<TWeakObjectPtr<UHoudiniCSV>> Objects) const
{
	for ( auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt )
	{
		auto Object = (*ObjIt).Get();
		if (!Object)
			continue;

		const FString SourceFilePath = Object->FileName;
		if ( SourceFilePath.Len() && IFileManager::Get().FileSize(*SourceFilePath) != INDEX_NONE )
			return FPlatformProcess::ExploreFolder(*SourceFilePath);
	}
}

#undef LOCTEXT_NAMESPACE
