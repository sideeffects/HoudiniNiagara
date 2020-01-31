#pragma once

#include "CoreMinimal.h"

#include "HoudiniPointCacheLoader.h"

class UHoudiniPointCache;

class FHoudiniPointCacheLoaderJSON : public FHoudiniPointCacheLoader
{
    public:
        FHoudiniPointCacheLoaderJSON(const FString& InFilePath);
        
        virtual bool LoadToAsset(UHoudiniPointCache *InAsset) override;

        // Markers
        static const TCHAR MarkerTypeNull;
        static const TCHAR MarkerTypeNoop;
        static const TCHAR MarkerTypeBoolTrue;
        static const TCHAR MarkerTypeBoolFalse;
        static const TCHAR MarkerTypeInt8;
        static const TCHAR MarkerTypeUInt8;
        static const TCHAR MarkerTypeInt16;
        static const TCHAR MarkerTypeInt32;
        static const TCHAR MarkerTypeInt64;
        static const TCHAR MarkerTypeFloat32;
        static const TCHAR MarkerTypeFloat64;
        static const TCHAR MarkerTypeHighPrec;
        static const TCHAR MarkerTypeChar;
        static const TCHAR MarkerTypeString;
        static const TCHAR MarkerObjectStart;
        static const TCHAR MarkerObjectEnd;
        static const TCHAR MarkerArrayStart;
        static const TCHAR MarkerArrayEnd;
        static const TCHAR MarkerContainerType;
        static const TCHAR MarkerContainerCount;

        bool ReadMarker(FArchive &InReader, TArray<uint8> &InBuffer, TCHAR &OutMarker) const;

        template <class T>
        bool ReadArray(FArchive &InReader, TArray<uint8> &InBuffer, TArray<T> &OutArray) const
        {
            // Expect array start
            TCHAR Marker = '\0';
            if (!ReadMarker(InReader, InBuffer, Marker) || Marker != MarkerArrayStart)
                return false;
            while (InBuffer[0] != MarkerArrayEnd && !InReader.AtEnd())
            {
                T Value;
                if (!ReadNonContainerValue(InReader, InBuffer, Value))
                {
                    if (InBuffer[0] == MarkerArrayEnd)
                        break;
                    return false;
                }
                else
                {
                    OutArray.Add(Value);
                }
            }

            if (InBuffer[0] != MarkerArrayEnd)
                return false;
            
            return true;
        }

        bool ReadHeader(FArchive &InReader, TArray<uint8> &InBuffer, struct FHoudiniPointCacheJSONHeader &OutHeader) const;

        template<class T>
        bool ReadNonContainerValue(FArchive &InReader, TArray<uint8> &InBuffer, T &OutValue, bool bInReadMarkerType=true, TCHAR InMarkerType='\0') const
        {
            uint32 Size = 0;
            TCHAR MarkerType = '\0';
            if (bInReadMarkerType)
            {
                // Read TYPE (1 byte)
                InReader.Serialize(InBuffer.GetData(), 1);
                MarkerType = InBuffer[0];
            }
            else
            {
                MarkerType = InMarkerType;
            }
            switch (MarkerType)
            {
                case MarkerTypeNull:
                    Size = 0;
                    break;
                case MarkerTypeNoop:
                    Size = 0;
                    break;
                case MarkerTypeBoolTrue:
                    OutValue = true;
                    return true;
                case MarkerTypeBoolFalse:
                    OutValue = false;
                    return true;
                case MarkerTypeInt8:
                    Size = 1;
                    break;
                case MarkerTypeUInt8:
                    Size = 1;
                    break;
                case MarkerTypeInt16:
                    Size = 2;
                    break;
                case MarkerTypeInt32:
                    Size = 4;
                    break;
                case MarkerTypeInt64:
                    Size = 8;
                    break;
                case MarkerTypeFloat32:
                    Size = 4;
                    break;
                case MarkerTypeFloat64:
                    Size = 8;
                    break;
                case MarkerTypeHighPrec:
                    Size = 8;
                    break;
                case MarkerTypeChar:
                    Size = 1;
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

            InReader.ByteOrderSerialize(InBuffer.GetData(), Size);

            switch (MarkerType)
            {
                case MarkerTypeInt8:
                    OutValue = static_cast<T>(*reinterpret_cast<int8*>(InBuffer.GetData()));
                    break;
                case MarkerTypeUInt8:
                    OutValue = static_cast<T>(*reinterpret_cast<uint8*>(InBuffer.GetData()));
                    break;
                case MarkerTypeInt16:
                    OutValue = static_cast<T>(*reinterpret_cast<int16*>(InBuffer.GetData()));
                    break;
                case MarkerTypeInt32:
                    OutValue = static_cast<T>(*reinterpret_cast<int32*>(InBuffer.GetData()));
                    break;
                case MarkerTypeInt64:
                    OutValue = static_cast<T>(*reinterpret_cast<int64*>(InBuffer.GetData()));
                    break;
                case MarkerTypeFloat32:
                    OutValue = static_cast<T>(*reinterpret_cast<float*>(InBuffer.GetData()));
                    break;
                case MarkerTypeFloat64:
                    OutValue = static_cast<T>(*reinterpret_cast<double*>(InBuffer.GetData()));
                    break;
                case MarkerTypeHighPrec:
                    OutValue = static_cast<T>(*reinterpret_cast<double*>(InBuffer.GetData()));
                    break;
                case MarkerTypeChar:
                    OutValue = static_cast<T>(*reinterpret_cast<unsigned char*>(InBuffer.GetData()));
                    break;
                default:
                    UE_LOG(LogHoudiniNiagara, Error, TEXT("Unhandled marker type %c"), MarkerType);
                    return false;
            }

            return true;
        }

        bool ReadNonContainerValue(FArchive &InReader, TArray<uint8> &InBuffer, FString &OutValue, bool bInReadMarkerType=true, TCHAR InMarkerType=MarkerTypeString) const;

        bool ParseAttributes(UHoudiniPointCache *InAsset, const struct FHoudiniPointCacheJSONHeader &InHeader, uint32 &OutNumAttributesPerFileSample);

    protected:

};