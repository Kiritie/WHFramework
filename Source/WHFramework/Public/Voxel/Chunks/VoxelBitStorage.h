#pragma once
#include "CoreMinimal.h"
class WHFRAMEWORK_API FVoxelBitStorage
{
public:
    bool Reset(uint8 InBits,int32 InCount);
    uint32 Get(int32 Index) const;
    void Set(int32 Index,uint32 Value);
    bool Import(uint8 InBits,int32 InCount,const TArray<uint64>& InWords);
    uint8 GetBits() const { return Bits; }
    const TArray<uint64>& GetWords() const { return Words; }
    void Clear();
private:
    uint8 Bits=0;
    int32 Count=0;
    uint64 Mask=0;
    TArray<uint64> Words;
};
