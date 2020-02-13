#pragma once

#include "CoreMinimal.h"

#include "HoudiniPointCacheLoader.h"

class UHoudiniPointCache;

class FHoudiniPointCacheLoaderCSV : public FHoudiniPointCacheLoader
{
    public:
        FHoudiniPointCacheLoaderCSV(const FString& InFilePath);
        
        virtual bool LoadToAsset(UHoudiniPointCache *InAsset) override;

    protected:
    	virtual bool UpdateFromStringArray(UHoudiniPointCache *InAsset, TArray<FString>& InStringArray);

    	// Parses the CSV title row to update the column indexes of special values we're interested in
    	// Also look for packed vectors in the first row and update the indexes accordingly
	    virtual bool ParseCSVTitleRow(UHoudiniPointCache *InAsset, const FString& TitleRow, const FString& FirstValueRow, bool& HasPackedVectors);

};