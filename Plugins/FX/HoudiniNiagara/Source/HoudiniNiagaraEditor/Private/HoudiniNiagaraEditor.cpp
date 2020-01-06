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

#include "HoudiniNiagaraEditor.h"
#include "HoudiniCSVAssetActions.h"
#include "AssetRegistryModule.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyle.h"

#define LOCTEXT_NAMESPACE "FHoudiniNiagaraModule"

void FHoudiniNiagaraEditorModule::StartupModule()
{
	// Register the Houdini CSV Type Actions
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked< FAssetToolsModule >("AssetTools").Get();

	TSharedRef< IAssetTypeActions > HCSVAction = MakeShareable( new FHoudiniCSVAssetActions() );
	AssetTools.RegisterAssetTypeActions( HCSVAction );
	AssetTypeActions.Add( HCSVAction );

	// Create Slate style set.
	if (!StyleSet.IsValid())
	{
		// Create Slate style set.	
		StyleSet = MakeShareable(new FSlateStyleSet(TEXT("HoudiniNiagaraStyle")));
		StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
		StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

		// Note, these sizes are in Slate Units. Slate Units do NOT have to map to pixels.
		const FVector2D Icon16x16(16.0f, 16.0f);
		const FVector2D Icon64x64(64.0f, 64.0f);
		const FVector2D Icon128x128(128.0f, 128.0f);
		
		static FString IconsDir = FPaths::EnginePluginsDir() / TEXT("FX/HoudiniNiagara/Resources/");

		// Register the Asset icon
		StyleSet->Set(
			"ClassIcon.HoudiniCSV",
			new FSlateImageBrush(IconsDir + TEXT("HCSVIcon128.png"), Icon16x16));

		StyleSet->Set(
			"ClassThumbnail.HoudiniCSV",
			new FSlateImageBrush(IconsDir + TEXT("HCSVIcon128.png"), Icon64x64));

		// Register Slate style.
		FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
	}
}

void FHoudiniNiagaraEditorModule::ShutdownModule()
{
	// Unregister asset type actions we have previously registered.
	if ( FModuleManager::Get().IsModuleLoaded("AssetTools") )
	{
		IAssetTools & AssetTools = FModuleManager::GetModuleChecked< FAssetToolsModule >("AssetTools").Get();

		for ( int32 Index = 0; Index < AssetTypeActions.Num(); ++Index )
			AssetTools.UnregisterAssetTypeActions( AssetTypeActions[Index].ToSharedRef() );

		AssetTypeActions.Empty();
	}

	// Unregister Slate style set.
	if (StyleSet.IsValid())
	{
		// Unregister Slate style.
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());

		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FHoudiniNiagaraEditorModule, HoudiniNiagaraEditor)
