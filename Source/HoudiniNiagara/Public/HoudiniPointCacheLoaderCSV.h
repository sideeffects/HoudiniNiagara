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