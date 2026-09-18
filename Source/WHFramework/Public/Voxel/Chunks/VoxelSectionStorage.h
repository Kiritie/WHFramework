#pragma once
#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/Chunks/VoxelBitStorage.h"
enum class EVoxelSectionStorageMode:uint8 { Uniform,Palette,Direct };
class WHFRAMEWORK_API FVoxelSectionStorage
{
public:
    FVoxelBlockState Get(uint16 I) const;
    void Set(uint16 I,FVoxelBlockState S);
    void Fill(FVoxelBlockState S);
    bool BuildFromDense(TConstArrayView<uint32> Values);
    void CopyToDense(TArray<uint32>& Out) const;
    void Compact();
    uint64 PayloadBytes() const;
    EVoxelSectionStorageMode GetMode()const{return Mode;}
private:
    void Repack(uint8 Bits);
    void MakeDirect();
    EVoxelSectionStorageMode Mode=EVoxelSectionStorageMode::Uniform;
    FVoxelBlockState Uniform;
    TArray<FVoxelBlockState> Palette;
    FVoxelBitStorage Indices;
    TArray<uint32> Direct;
    uint32 WritesSinceCompact=0;
};
