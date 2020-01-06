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

#pragma once

#include "AssetTypeActions_Base.h"
#include "Templates/SharedPointer.h"

class UHoudiniCSV;


class FHoudiniCSVAssetActions : public FAssetTypeActions_Base
{
    public:

	FHoudiniCSVAssetActions();

    public:

	//~ FAssetTypeActions_Base overrides
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;    
	virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override;
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;

	virtual bool CanFilter() override;   
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override;

    private:

	// Reimport actions
	bool CanExecuteReimport(const TArray<TWeakObjectPtr<UHoudiniCSV>> Objects) const;
	void ExecuteReimport(const TArray<TWeakObjectPtr<UHoudiniCSV>> Objects) const;

	// Open In Editor Actions
	bool CanExecuteOpenInEditor(const TArray<TWeakObjectPtr<UHoudiniCSV>> Objects) const;
	void ExecuteOpenInEditor(const TArray<TWeakObjectPtr<UHoudiniCSV>> Objects) const;

	// Find In Explorer Actions
	bool CanExecuteFindInExplorer(const TArray<TWeakObjectPtr<UHoudiniCSV>> Objects) const;
	void ExecuteFindInExplorer(const TArray<TWeakObjectPtr<UHoudiniCSV>> Objects) const;	
};
