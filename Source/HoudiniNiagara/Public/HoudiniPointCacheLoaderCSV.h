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

#include "CoreMinimal.h"
#include "HoudiniPointCacheLoader.h"

class UHoudiniPointCache;

class FHoudiniPointCacheLoaderCSV : public FHoudiniPointCacheLoader
{
    public:
        FHoudiniPointCacheLoaderCSV(const FString& InFilePath);

#if WITH_EDITOR
        virtual bool LoadToAsset(UHoudiniPointCache *InAsset) override;

		virtual FName GetFormatID() const override { return "HCSV"; };
#endif

    protected:
	
#if WITH_EDITOR
    	virtual bool UpdateFromStringArray(UHoudiniPointCache *InAsset, TArray<FString>& InStringArray);

    	// Parses the CSV title row to update the column indexes of special values we're interested in
    	// Also look for packed vectors in the first row and update the indexes accordingly
	    virtual bool ParseCSVTitleRow(UHoudiniPointCache *InAsset, const FString& TitleRow, const FString& FirstValueRow, bool& HasPackedVectors);
#endif
};