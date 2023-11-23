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

#include "HoudiniPointCacheExporterBase.h"
#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "HoudiniPointCache.h"

UHoudiniPointCacheExporterBase::UHoudiniPointCacheExporterBase()
{
	
}

bool UHoudiniPointCacheExporterBase::SupportsObject(UObject* Object) const
{
	if (!UExporter::SupportsObject(Object))
		return false; // If the object doesn't match the SupportedClass, return false.

	// Further, ensure that the asset contains the source data needed for exporting.
	UHoudiniPointCache* PointCache = Cast<UHoudiniPointCache>(Object);
	if (!PointCache)
		return false;

	if (!IsRawFormatSupported(PointCache))
	{
		return false;
	}
	
	return PointCache->HasRawData();
}

bool UHoudiniPointCacheExporterBase::ExportBinary(UObject* Object, const TCHAR* Type, FArchive& Ar,
	FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags)
{
	UHoudiniPointCache* PointCache = Cast<UHoudiniPointCache>(Object);
	if (!PointCache)
	{
		return false;
	}
	
	if (!PointCache->HasRawData())
	{
		return false;
	}

	if (PointCache->RawDataCompressionMethod.IsEqual(NAME_None))
	{
		return false;
	}

	// Uncompress data before serialization
	const uint32 UncompressedSize = PointCache->RawDataUncompressedSize;

	int32 CompressedSize = PointCache->RawDataCompressed.Num();
	TArray<uint8> UncompressedData;
	UncompressedData.SetNumUninitialized(UncompressedSize);
	
	if (FCompression::UncompressMemory(
		PointCache->RawDataCompressionMethod,
		UncompressedData.GetData(),
		UncompressedSize,
		PointCache->RawDataCompressed.GetData(),
		PointCache->RawDataCompressed.Num()))
	{
		Ar.Serialize(UncompressedData.GetData(), UncompressedSize);
	}

	return true;
}
