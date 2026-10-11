#pragma once

#include "CoreMinimal.h"

class WHFRAMEWORK_API FInventoryBinaryWriter
{
public:
    explicit FInventoryBinaryWriter(int32 InLimit);
    void U8(uint8 InValue);
    void U32(uint32 InValue);
    void U64(uint64 InValue);
    void Guid(const FGuid& InValue);
    void String(const FString& InValue, int32 InMaximumBytes = 512);
    void Name(FName InValue, int32 InMaximumBytes = 512);
    void Blob(TConstArrayView<uint8> InValue, int32 InMaximumBytes);
    void Raw(TConstArrayView<uint8> InValue);
    bool IsValid() const;
    const TArray<uint8>& GetBytes() const;
    TArray<uint8> TakeBytes();

private:
    TArray<uint8> Bytes;
    int32 Limit;
    bool bValid = true;
};

class WHFRAMEWORK_API FInventoryBinaryReader
{
public:
    explicit FInventoryBinaryReader(TConstArrayView<uint8> InBytes);
    uint8 U8();
    uint32 U32();
    uint64 U64();
    FGuid Guid();
    FString String(int32 InMaximumBytes = 512);
    FName Name(int32 InMaximumBytes = 512);
    TArray<uint8> Blob(int32 InMaximumBytes);
    TConstArrayView<uint8> Raw(int32 InCount);
    bool IsValid() const;
    bool AtEnd() const;
    int32 Remaining() const;
    void Reject();

private:
    TConstArrayView<uint8> Bytes;
    int32 Position = 0;
    bool bValid = true;
};
