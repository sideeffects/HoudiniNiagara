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
#include "HoudiniCSVAssetActions.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HoudiniCSV.h"
#include "Styling/SlateStyle.h"

//#include "HoudiniCSVAssetEditorToolkit.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"



FHoudiniCSVAssetActions::FHoudiniCSVAssetActions(const TSharedRef<ISlateStyle>& InStyle)
    : Style(InStyle)
{ }

bool FHoudiniCSVAssetActions::CanFilter()
{
    return true;
}


void FHoudiniCSVAssetActions::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
    FAssetTypeActions_Base::GetActions(InObjects, MenuBuilder);

    auto HoudiniCSVAssets = GetTypedWeakObjectPtrs<UHoudiniCSV>(InObjects);

    /*MenuBuilder.AddMenuEntry(
	LOCTEXT("TextAsset_ReverseText", "Reverse Text"),
	LOCTEXT("TextAsset_ReverseTextToolTip", "Reverse the text stored in the selected text asset(s)."),
	FSlateIcon(),
	FUIAction(
	    FExecuteAction::CreateLambda([=] {
	for (auto& TextAsset : TextAssets)
	{
	    if (TextAsset.IsValid() && !TextAsset->Text.IsEmpty())
	    {
		TextAsset->Text = FText::FromString(TextAsset->Text.ToString().Reverse());
		TextAsset->PostEditChange();
		TextAsset->MarkPackageDirty();
	    }
	}
    }),
	    FCanExecuteAction::CreateLambda([=] {
	for (auto& TextAsset : TextAssets)
	{
	    if (TextAsset.IsValid() && !TextAsset->Text.IsEmpty())
	    {
		return true;
	    }
	}
	return false;
    })
	)
    );*/
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
    return FText::GetEmpty();
}


FColor FHoudiniCSVAssetActions::GetTypeColor() const
{
    return FColor::Orange;
}


bool FHoudiniCSVAssetActions::HasActions(const TArray<UObject*>& InObjects) const
{
    return true;
}

/*
void FHoudiniCSVAssetActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
    EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid()
	? EToolkitMode::WorldCentric
	: EToolkitMode::Standalone;

    for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
    {
	auto HoudiniCSVAsset = Cast<UHoudiniCSV>(*ObjIt);

	if (HoudiniCSVAsset != nullptr)
	{
	    TSharedRef<FHoudiniCSVAssetEditorToolkit> EditorToolkit = MakeShareable(new FHoudiniCSVAssetEditorToolkit(Style));
	    EditorToolkit->Initialize(HoudiniCSVAsset, Mode, EditWithinLevelEditor);
	}
    }
}
*/


#undef LOCTEXT_NAMESPACE