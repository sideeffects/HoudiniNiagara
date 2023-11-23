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
