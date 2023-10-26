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
#include "HoudiniPointCacheLoaderJSONBase.h"

class UHoudiniPointCache;

/** This is loader for Houdini Point cache data from a custom binary JSON-like format. 
 * See HoudiniPointCacheLoaderJSON.h for the text spec.
 * 
 * Similar to the text JSON format objects are start and ended with a byte { or } and arrays
 * with [ ]. The data type of each field is known/fixed. Field names are present in the
 * file as strings, and strings are read as [size_type_marker:1 byte][size][string_data]. No other entries
 * in the file are prefixed with types or sizes. The point data attribute frame
 * sample array value types are determined by 'attrib_data_type' (see the various Marker* definitions) 
 * in the header.
*/
class FHoudiniPointCacheLoaderBJSON : public FHoudiniPointCacheLoaderJSONBase
{
    public:
        /** Construct with the input file path. */
        FHoudiniPointCacheLoaderBJSON(const FString& InFilePath);

#if WITH_EDITOR
        /** Load the data from FilePath into a UHoudiniPointCache asset.
         * @return false on errors, true otherwise.
         */
        virtual bool LoadToAsset(UHoudiniPointCache *InAsset) override;

        virtual FName GetFormatID() const override { return "HBJSON"; };
#endif

        // Strings are written with [size_type_marker][size][string_data]
        // The header also contains a field 'attrib_data_type' that defines
        // data type of each attribute used in sample data. These markers
        // define the various supported data types, along with some special
        // markers for the start/end of objects and arrays.
        static const unsigned char MarkerTypeChar = 'c';
        static const unsigned char MarkerTypeInt8 = 'b';
        static const unsigned char MarkerTypeUInt8 = 'B';
        static const unsigned char MarkerTypeBool = '?';
        static const unsigned char MarkerTypeInt16 = 'h';
        static const unsigned char MarkerTypeUInt16 = 'H';
        static const unsigned char MarkerTypeInt32 = 'l';
        static const unsigned char MarkerTypeUInt32 = 'L';
        static const unsigned char MarkerTypeInt64 = 'q';
        static const unsigned char MarkerTypeUInt64 = 'Q';
        static const unsigned char MarkerTypeFloat32 = 'f';
        static const unsigned char MarkerTypeFloat64 = 'd';
        static const unsigned char MarkerTypeString = 's';
        static const unsigned char MarkerObjectStart = '{';
        static const unsigned char MarkerObjectEnd = '}';
        static const unsigned char MarkerArrayStart = '[';
        static const unsigned char MarkerArrayEnd = ']';

        /** Read the next `InSize` bytes into `Buffer` but leave the reader at the starting point. */
        bool Peek(int32 InSize);

        /** Check if `InNext` is the next data that will be read from `Reader`. */
        template<class T>
        inline bool IsNext(const T &InNext) 
        {
            const bool bResult = (Peek(sizeof(T)) && (*reinterpret_cast<T*>(Buffer.GetData())) == InNext);
            return bResult;
        }

        /** Read a type marker from `Reader`. */
        bool ReadMarker(unsigned char &OutMarker);

        /** Read an array from the file. To treat each entry in the array as the same data type, set InSingleMarkerType. To specify
         * a distinct data type for each array element, pass an array of markers as InMarkerTypes.
         */
        template <class T>
        bool ReadArray(TArray<T> &OutArray, unsigned char InSingleMarkerType='\0', const TArray<unsigned char> *InMarkerTypes=nullptr)
        {
            // Expect array start
            unsigned char Marker = '\0';
            if (!ReadMarker(Marker) || Marker != MarkerArrayStart)
                return false;
            // Read until we reach MarkerArrayEnd or EOF
            int32 Index = 0;
            while (!Reader->AtEnd() && !IsNext(MarkerArrayEnd))
            {
                T Value;
                // Determine the data type, which in turn determines the number of bytes to
                // read next. If InSingleMarkerType is set and InMarkerTypes is not set, then
                // read everything as InSingleMarkerType data.
                unsigned char MarkerType = InSingleMarkerType;
                if (InMarkerTypes && Index < InMarkerTypes->Num())
                {
                    MarkerType = InMarkerTypes->GetData()[Index];
                }
                if (!ReadNonContainerValue(Value, false, MarkerType))
                {
                    return false;
                }
                else
                {
                    OutArray.Add(Value);
                }
                Index++;
            }

            // Consume the end of array marker
            if (!ReadMarker(Marker) || Marker != MarkerArrayEnd)
                return false;
            
            return true;
        }

        /** Read the header from `Reader` and populate `OutHeader`. */
        bool ReadHeader(struct FHoudiniPointCacheJSONHeader &OutHeader);

        /** Read a plain old data type, no objects or arrays. If `bInReadMarkerType` is true, then first read one byte to determine the data type
         * to expect. Otherwise pass `InMarkerType` to specify the data type, and thus the size, to read from `Reader`. Data is read and interpreted 
         * according to InMarkerType (or the detected marker) and then cast to T.
         */
        template<class T>
        bool ReadNonContainerValue(T &OutValue, bool bInReadMarkerType=false, unsigned char InMarkerType='\0')
        {
            uint32 Size = 0;
            unsigned char MarkerType = '\0';

            if (!CheckReader())
                return false;

            if (bInReadMarkerType)
            {
                // Read TYPE (1 byte)
                Reader->Serialize(Buffer.GetData(), 1);
                MarkerType = Buffer[0];
            }
            else
            {
                MarkerType = InMarkerType;
            }

            // Determine the size to read, in bytes, based on MarkerType
            switch (MarkerType)
            {
                case '\0':
                    Size = sizeof(T);
                    break;
                case MarkerTypeChar:
                    Size = 1;
                    break;
                case MarkerTypeInt8:
                case MarkerTypeUInt8:
                case MarkerTypeBool:
                    Size = 1;
                    break;
                case MarkerTypeInt16:
                case MarkerTypeUInt16:
                    Size = 2;
                    break;
                case MarkerTypeInt32:
                case MarkerTypeUInt32:
                    Size = 4;
                    break;
                case MarkerTypeInt64:
                case MarkerTypeUInt64:
                    Size = 8;
                    break;
                case MarkerTypeFloat32:
                    Size = 4;
                    break;
                case MarkerTypeFloat64:
                    Size = 8;
                    break;
                default:
                    UE_LOG(LogHoudiniNiagara, Error, TEXT("Unknown marker type %c"), MarkerType);
                    return false;
            }

            if (Size > sizeof(T))
            {
                UE_LOG(LogHoudiniNiagara, Error, TEXT("Could not read value of %d bytes into %d byte target."), Size, sizeof(T));
                return false;
            }

            FMemory::Memzero(OutValue);
            if (Size == 0)
                return true;

            Reader->ByteOrderSerialize(Buffer.GetData(), Size);

            // Interpret data in the type associated with the marker and then cast to T
            switch (MarkerType)
            {
                case '\0':
                    OutValue = *reinterpret_cast<T*>(Buffer.GetData());
                    break;
                case MarkerTypeChar:
                    OutValue = static_cast<T>(*reinterpret_cast<unsigned char*>(Buffer.GetData()));
                    break;
                case MarkerTypeInt8:
                    OutValue = static_cast<T>(*reinterpret_cast<int8*>(Buffer.GetData()));
                    break;
                case MarkerTypeUInt8:
                    OutValue = static_cast<T>(*reinterpret_cast<uint8*>(Buffer.GetData()));
                    break;
                case MarkerTypeBool:
                    OutValue = static_cast<T>(*reinterpret_cast<bool*>(Buffer.GetData()));
                    break;
                case MarkerTypeInt16:
                    OutValue = static_cast<T>(*reinterpret_cast<int16*>(Buffer.GetData()));
                    break;
                case MarkerTypeUInt16:
                    OutValue = static_cast<T>(*reinterpret_cast<uint16*>(Buffer.GetData()));
                    break;
                case MarkerTypeInt32:
                    OutValue = static_cast<T>(*reinterpret_cast<int32*>(Buffer.GetData()));
                    break;
                case MarkerTypeUInt32:
                    OutValue = static_cast<T>(*reinterpret_cast<uint32*>(Buffer.GetData()));
                    break;
                case MarkerTypeInt64:
                    OutValue = static_cast<T>(*reinterpret_cast<int64*>(Buffer.GetData()));
                    break;
                case MarkerTypeUInt64:
                    OutValue = static_cast<T>(*reinterpret_cast<uint64*>(Buffer.GetData()));
                    break;
                case MarkerTypeFloat32:
                    OutValue = static_cast<T>(*reinterpret_cast<float*>(Buffer.GetData()));
                    break;
                case MarkerTypeFloat64:
                    OutValue = static_cast<T>(*reinterpret_cast<double*>(Buffer.GetData()));
                    break;
                default:
                    UE_LOG(LogHoudiniNiagara, Error, TEXT("Unhandled marker type %c"), MarkerType);
                    return false;
            }

            return true;
        }

        /** Read a string from `Reader`. */
        bool ReadNonContainerValue(FString &OutValue, bool bInReadMarkerType=false, unsigned char InMarkerType=MarkerTypeString);

    protected:
        // File stream
        TUniquePtr<FArchive> Reader;
        // Buffer that is used to store data that was read from the Reader and is being processed.
        TArray<uint8> Buffer;

        // Checks if Reader is valid and not null, if not, log an error and return false
        bool CheckReader(bool bInCheckAtEnd=true) const;
};
