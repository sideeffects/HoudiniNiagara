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

#include "HoudiniPointCacheExporterHBJSON.h"

#include "HoudiniPointCache.h"

#include "CoreMinimal.h"
#include "Misc/Paths.h"

UHoudiniPointCacheExporterHBJSON::UHoudiniPointCacheExporterHBJSON()
{
	SupportedClass = UHoudiniPointCache::StaticClass();
	bText = false;
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("hbjson"));
	FormatDescription.Add(TEXT("HoudiniPointCache HBJSON File"));
}

bool UHoudiniPointCacheExporterHBJSON::IsRawFormatSupported(UHoudiniPointCache* PointCache) const
{
	if (!Super::IsRawFormatSupported(PointCache))
		return false;
	const bool bIsSupported = PointCache->RawDataFormatID.IsEqual("HBJSON");
	return bIsSupported;
}

