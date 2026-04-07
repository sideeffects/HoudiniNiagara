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

#include "HoudiniPointCacheLoader.h"

#include "HoudiniPointCache.h"

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Compression.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"

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

FHoudiniPointCacheLoader::FHoudiniPointCacheLoader(const FString& InFilePath)
    : FilePath(InFilePath)
{

}

FHoudiniPointCacheLoader::~FHoudiniPointCacheLoader()
{
    
}

#if WITH_EDITOR
bool 
FHoudiniPointCacheLoader::LoadRawPointCacheData(
	UHoudiniPointCache* InAsset, 
	const FString& InFilePath,
	TArray<uint8, FDefaultAllocator64>& BufferData) const
{
    InAsset->Modify();

    IFileManager& FileManager = IFileManager::Get();
    if (!FileManager.FileExists(*FilePath))
    {
        return false;
    }
    int64 Size = FileManager.FileSize(*FilePath);
    if (Size + 2 > MAX_int32)
    {
        // Large file - mark the point cache as such
        InAsset->bLargeFile = true;
    }

    // Load data into the TArray64
    return FFileHelper::LoadFileToArray(BufferData, *InFilePath);
}
#endif


#if WITH_EDITOR
void
FHoudiniPointCacheLoader::CompressRawData(
	UHoudiniPointCache* InAsset,
	const TArray<uint8, FDefaultAllocator64>& BufferData) const
{
    constexpr ECompressionFlags CompressFlags = COMPRESS_BiasMemory;
    const int64 UncompressedSize = BufferData.Num();

    // Skip compression for files larger than 2GB due to internal FCompression limitations
    // Even though the API accepts int64, there are internal int32 checks that will fail
    if ((UncompressedSize > INT32_MAX) || InAsset->bLargeFile)
    {
        UE_LOG(LogHoudiniNiagara, Warning, TEXT("Skipping compression for large file (%lld bytes). Source raw data for the Point Cache will not be stored - export disabled."), UncompressedSize);
        InAsset->RawDataUncompressedSize = UncompressedSize;
        InAsset->RawDataCompressionMethod = NAME_None;
        InAsset->RawDataFormatID = GetFormatID();
        return;
    }

    const FName CompressionName = NAME_Oodle;
	int32 CompressedSize = FCompression::CompressMemoryBound(CompressionName, UncompressedSize, CompressFlags);
    InAsset->RawDataCompressed.SetNum(CompressedSize);

	if (FCompression::CompressMemory(
	    CompressionName,
        InAsset->RawDataCompressed.GetData(),
	    CompressedSize,
	    BufferData.GetData(),
	    UncompressedSize,
	    CompressFlags))
	{
        InAsset->RawDataCompressed.SetNum(CompressedSize);
        InAsset->RawDataCompressed.Shrink();
	}
    
    InAsset->RawDataUncompressedSize = UncompressedSize;
    InAsset->RawDataFormatID = GetFormatID();
}
#endif
