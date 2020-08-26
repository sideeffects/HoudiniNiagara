#include "HoudiniPointCacheLoader.h"
#include "HoudiniPointCache.h"


FHoudiniPointCacheSortPredicate::FHoudiniPointCacheSortPredicate(const int32 &InTimeAttrIndex, const int32 &InAgeAttrIndex, const int32 &InIDAttrIndex )
    : TimeAttributeIndex( InTimeAttrIndex ), AgeAttributeIndex(InAgeAttrIndex), IDAttributeIndex( InIDAttrIndex )
{

}

bool FHoudiniPointCacheSortPredicate::operator()( const TArray<FString>& A, const TArray<FString>& B ) const
{
    float ATime = TNumericLimits< float >::Lowest();
    if ( A.IsValidIndex( TimeAttributeIndex ) )
        ATime = FCString::Atof( *A[ TimeAttributeIndex ] );

    float BTime = TNumericLimits< float >::Lowest();
    if ( B.IsValidIndex( TimeAttributeIndex ) )
        BTime = FCString::Atof( *B[ TimeAttributeIndex ] );

    if ( ATime != BTime )
    {
        return ATime < BTime;
    }
    else
    {
        float AAge = TNumericLimits< float >::Lowest();
        if (A.IsValidIndex(AgeAttributeIndex))
            AAge = FCString::Atof(*A[AgeAttributeIndex]);

        float BAge = TNumericLimits< float >::Lowest();
        if (B.IsValidIndex(AgeAttributeIndex))
            BAge = FCString::Atof(*B[AgeAttributeIndex]);

        if (AAge != BAge)
        {
            return BAge < AAge;
        }
        else
        {
            float AID = TNumericLimits< float >::Lowest();
            if (A.IsValidIndex(IDAttributeIndex))
                AID = FCString::Atof(*A[IDAttributeIndex]);

            float BID = TNumericLimits< float >::Lowest();
            if (B.IsValidIndex(IDAttributeIndex))
                BID = FCString::Atof(*B[IDAttributeIndex]);

            return AID <= BID;
        }
    }
}


FHoudiniPointCacheLoader::FHoudiniPointCacheLoader(const FString& InFilePath) :
    FilePath(InFilePath)
{

}

FHoudiniPointCacheLoader::~FHoudiniPointCacheLoader()
{
    
}
