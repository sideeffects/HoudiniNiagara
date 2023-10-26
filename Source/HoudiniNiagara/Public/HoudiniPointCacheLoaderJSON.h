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
#include "Dom/JsonObject.h"
#include "HoudiniPointCacheLoaderJSONBase.h"

class UHoudiniPointCache;

/**
 * Text JSON Houdini Point Cache loader using FJsonReader and FJsonSerializer.
 * An example:
 * {
 *      "header" : {
 *          "version" : 1.0,
 *          "num_samples" : 100,  (?? needed ??)
 *          “num_frames” : 10,
 *          “num_points” : 10,
 *          “num_attrib” : 5
 *          “attrib_name” : [ "id", "P", “N”, “Cd”, “age” ]
 *          “attrib_size” : [ 1, 3, 3, 4, 1 ]
 *          “data_type” : “linear”,
 *      },
 *      “cache_data” : {
 *          “frames” : [
 *              {
 *                  “number” : 0
 *                  “time” : 0.0
 *                  “frame_data” : [
 *                      [0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]
 *                      [1, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]
 *                      [2, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]
 *                      [3, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]
 *                      [4, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]
 *                      [5, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]
 *                      [6, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]
 *                      [7, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]
 *                      [8, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]
 *                      [9, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]		
 *                  ]
 *              },
 *              {
 *                  “number” : 1
 *                  “time” : 0.033333
 *                  “frame_data” : {
 *                      [0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]
 *                      [1, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]
 *                      [2, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]
 *                      [3, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]
 *                      [4, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]
 *                      [5, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]
 *                      [6, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]
 *                      [7, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]
 *                      [8, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]
 *                      [9, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,1.0, 1.0,1.0, 1.0, 0.0 ]		
 *                  }
 *              }
 *          ]
 *      }
 * }
 */
class FHoudiniPointCacheLoaderJSON : public FHoudiniPointCacheLoaderJSONBase
{
    public:
        FHoudiniPointCacheLoaderJSON(const FString& InFilePath);

#if WITH_EDITOR
        virtual bool LoadToAsset(UHoudiniPointCache *InAsset) override;

		virtual FName GetFormatID() const override { return "HJSON"; };
#endif

        /** Read and process the 'header' object from the JSON InPointCacheObject and populate OutHeader. */
        bool ReadHeader(const FJsonObject &InPointCacheObject, FHoudiniPointCacheJSONHeader &OutHeader) const;
};
