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

#include "AssetTypeActions_Base.h"
#include "Templates/SharedPointer.h"

class ISlateStyle;

/**
* Implements an action for UHoudiniCSV assets.
*/
class FHoudiniCSVAssetActions : public FAssetTypeActions_Base
{
public:

    /**
    * Creates and initializes a new instance.
    *
    * @param InStyle The style set to use for asset editor toolkits.
    */
    FHoudiniCSVAssetActions(const TSharedRef<ISlateStyle>& InStyle);

public:

    //~ FAssetTypeActions_Base overrides
    virtual bool CanFilter() override;
    virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
    virtual uint32 GetCategories() override;
    virtual FText GetName() const override;
    virtual UClass* GetSupportedClass() const override;
    virtual FColor GetTypeColor() const override;
    virtual FText GetAssetDescription(const FAssetData& AssetData) const override;
    virtual bool HasActions(const TArray<UObject*>& InObjects) const override;
    //virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;

private:

    /** Pointer to the style set to use for toolkits. */
    TSharedRef<ISlateStyle> Style;
};