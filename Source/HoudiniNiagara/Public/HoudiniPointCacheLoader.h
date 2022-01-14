#pragma once

#include "CoreMinimal.h"
#include "HoudiniPointCache.h"

class UHoudiniPointCache;

struct FHoudiniPointCacheSortPredicate
{
	FHoudiniPointCacheSortPredicate(const int32 &InTimeAttrIndex, const int32 &InAgeAttrIndex, const int32 &InIDAttrIndex );

    template<class T>
	bool operator()( const TArray<T>& A, const TArray<T>& B ) const
    {
        float ATime = TNumericLimits< float >::Lowest();
        if ( A.IsValidIndex( TimeAttributeIndex ) )
            ATime = A[ TimeAttributeIndex ];

        float BTime = TNumericLimits< float >::Lowest();
        if ( B.IsValidIndex( TimeAttributeIndex ) )
            BTime = B[ TimeAttributeIndex ];

        if ( ATime != BTime )
        {
            return ATime < BTime;
        }
        else
        {
            float AAge = TNumericLimits< float >::Lowest();
            if (A.IsValidIndex(AgeAttributeIndex))
                AAge = A[AgeAttributeIndex];

            float BAge = TNumericLimits< float >::Lowest();
            if (B.IsValidIndex(AgeAttributeIndex))
                BAge = B[AgeAttributeIndex];

            if (AAge != BAge)
            {
                return BAge < AAge;
            }
            else
            {
                float AID = TNumericLimits< float >::Lowest();
                if (A.IsValidIndex(IDAttributeIndex))
                    AID = A[IDAttributeIndex];

                float BID = TNumericLimits< float >::Lowest();
                if (B.IsValidIndex(IDAttributeIndex))
                    BID = B[IDAttributeIndex];

                return AID <= BID;
            }
        }
    }

	bool operator()( const TArray<FString>& A, const TArray<FString>& B ) const;

	int32 TimeAttributeIndex;
	int32 AgeAttributeIndex;
	int32 IDAttributeIndex;
};


/**
 * This class is a base class for file loaders for the HoudiniPointCache asset.
 */
class FHoudiniPointCacheLoader
{
    public:
        FHoudiniPointCacheLoader(const FString& InFilePath);

        virtual ~FHoudiniPointCacheLoader();

        template <class T>
	    static TSharedPtr<T> Create(const FString& InFilePath)
	    {
            FHoudiniPointCacheLoader *Instance = new T(InFilePath);
		    return MakeShareable(static_cast<T*>(Instance));
	    }


#if WITH_EDITOR
        /**
         * Load the data from FilePath to InAsset.
         * Returns false on failure.
         */
    	virtual bool LoadToAsset(UHoudiniPointCache *InAsset) = 0;

        virtual FName GetFormatID() const { return NAME_None; };

        const FString& GetFilePath() const { return FilePath; }
#endif

    protected:

#if WITH_EDITOR
        bool LoadRawPointCacheData(UHoudiniPointCache* InAsset, const FString& InFilePath) const;
        void CompressRawData(UHoudiniPointCache* InAsset) const;
#endif

    private:
        /** The file to load from. */
        FString FilePath;

};
