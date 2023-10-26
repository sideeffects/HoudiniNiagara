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
#include "HoudiniPointCacheAssetActions.h"

#include "ToolMenus.h"
#include "HoudiniPointCache.h"
#include "EditorStyleSet.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "EditorReimportHandler.h"
#include "HAL/FileManager.h"

//#include "HoudiniPointCacheAssetEditorToolkit.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"


FHoudiniPointCacheAssetActions::FHoudiniPointCacheAssetActions()
{ }

bool FHoudiniPointCacheAssetActions::CanFilter()
{
    return true;
}


void FHoudiniPointCacheAssetActions::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
    FAssetTypeActions_Base::GetActions(InObjects, Section);

    auto HoudiniPointCacheAssets = GetTypedWeakObjectPtrs<UHoudiniPointCache>(InObjects);

    Section.AddMenuEntry(
		"ReimportHoudiniPointCacheLabel",
		LOCTEXT("ReimportHoudiniPointCacheLabel", "Reimport"),
		LOCTEXT("ReimportHoudiniPointCacheTooltip", "Reimport the selected Houdini Point Cache file(s)."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.ReimportAsset"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FHoudiniPointCacheAssetActions::ExecuteReimport, HoudiniPointCacheAssets),
			FCanExecuteAction::CreateSP(this, &FHoudiniPointCacheAssetActions::CanExecuteReimport, HoudiniPointCacheAssets)
		)
    );

    Section.AddMenuEntry(
		"OpenHoudiniPointCacheLabel",
		LOCTEXT("OpenHoudiniPointCacheLabel", "Open in Text Editor"),
		LOCTEXT("OpenHoudiniPointCacheTooltip", "Open the selected Houdini Point Cache file(s) in a Text Editor."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.OpenInExternalEditor"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FHoudiniPointCacheAssetActions::ExecuteOpenInEditor, HoudiniPointCacheAssets),
			FCanExecuteAction::CreateSP(this, &FHoudiniPointCacheAssetActions::CanExecuteOpenInEditor, HoudiniPointCacheAssets)
		)
    );

	Section.AddMenuEntry(
		"FindHoudiniPointCacheInExplorer",
		LOCTEXT("FindHoudiniPointCacheInExplorer", "Find Source File"),
		LOCTEXT("FindHoudiniPointCacheInExplorerTooltip", "Opens explorer at the location of this asset's source file."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.OpenInExternalEditor"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FHoudiniPointCacheAssetActions::ExecuteFindInExplorer, HoudiniPointCacheAssets),
			FCanExecuteAction::CreateSP(this, &FHoudiniPointCacheAssetActions::CanExecuteFindInExplorer, HoudiniPointCacheAssets)
		)
	);
}


uint32 FHoudiniPointCacheAssetActions::GetCategories()
{
    return EAssetTypeCategories::Misc;
}


FText FHoudiniPointCacheAssetActions::GetName() const
{
    return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_HoudiniPointCache", "Houdini Point Cache Asset");
}


UClass* FHoudiniPointCacheAssetActions::GetSupportedClass() const
{
    return UHoudiniPointCache::StaticClass();
}

FText FHoudiniPointCacheAssetActions::GetAssetDescription(const FAssetData& AssetData) const
{
	/*
	UHoudiniPointCache* PointCacheAsset = Cast<UHoudiniPointCache>(AssetData.GetAsset());
	if ( !PointCacheAsset )
		return FText::GetEmpty();

	FString StrDescription;
	StrDescription += TEXT("Number of Rows: ") + FString::FromInt(PointCacheAsset->NumberOfRows) + TEXT("\n");
	StrDescription += TEXT("Number of Columns: ") + FString::FromInt(PointCacheAsset->NumberOfColumns) + TEXT("\n");
	StrDescription += TEXT("Number of Points: ") + FString::FromInt(PointCacheAsset->NumberOfPoints) + TEXT("\n");

	return FText::FromString(StrDescription);
	*/
	return FText::GetEmpty();
}


FColor FHoudiniPointCacheAssetActions::GetTypeColor() const
{
    //return FColor::Orange;
	return FColor(255, 165, 0);
}


bool FHoudiniPointCacheAssetActions::HasActions(const TArray<UObject*>& InObjects) const
{
    return true;
}


class UThumbnailInfo* FHoudiniPointCacheAssetActions::GetThumbnailInfo(UObject* Asset) const
{
    /*
    UHoudiniPointCache* HoudiniPointCache = CastChecked<UHoudiniPointCache>(Asset);
    UThumbnailInfo* ThumbnailInfo = HoudiniPointCache->ThumbnailInfo;
    if (ThumbnailInfo == NULL)
    {
	ThumbnailInfo = NewObject<USceneThumbnailInfo>(HoudiniPointCache, NAME_None, RF_Transactional);
	HoudiniPointCache->ThumbnailInfo = ThumbnailInfo;
    }

    return ThumbnailInfo;
    */

    return nullptr;
}


bool FHoudiniPointCacheAssetActions::CanExecuteReimport(const TArray<TWeakObjectPtr<UHoudiniPointCache>> Objects) const
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

void FHoudiniPointCacheAssetActions::ExecuteReimport(const TArray<TWeakObjectPtr<UHoudiniPointCache>> Objects) const
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
FHoudiniPointCacheAssetActions::CanExecuteOpenInEditor(const TArray<TWeakObjectPtr<UHoudiniPointCache>> Objects) const
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
FHoudiniPointCacheAssetActions::ExecuteOpenInEditor(const TArray<TWeakObjectPtr<UHoudiniPointCache>> Objects) const
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
FHoudiniPointCacheAssetActions::CanExecuteFindInExplorer(const TArray<TWeakObjectPtr<UHoudiniPointCache>> Objects) const
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
FHoudiniPointCacheAssetActions::ExecuteFindInExplorer(const TArray<TWeakObjectPtr<UHoudiniPointCache>> Objects) const
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
