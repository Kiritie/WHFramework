#pragma once
#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelBlockState.h"
struct WHFRAMEWORK_API FVoxelSectionKey
{
    int32 X=0,Y=0,Z=0;
    bool operator==(const FVoxelSectionKey& B) const { return X==B.X&&Y==B.Y&&Z==B.Z; }
    bool operator!=(const FVoxelSectionKey& B) const { return !(*this==B); }
    bool operator<(const FVoxelSectionKey& B) const
    { return X!=B.X ? X<B.X : Y!=B.Y ? Y<B.Y : Z<B.Z; }
    friend uint32 GetTypeHash(const FVoxelSectionKey& K)
    { return HashCombineFast(HashCombineFast(::GetTypeHash(K.X),::GetTypeHash(K.Y)),::GetTypeHash(K.Z)); }
};
namespace VoxelCoord
{
    WHFRAMEWORK_API int32 FloorDiv(int32 V,int32 D);
    WHFRAMEWORK_API bool IsValid(const FIntVector& P);
    WHFRAMEWORK_API FVoxelSectionKey Section(const FIntVector& P);
    WHFRAMEWORK_API FIntVector Origin(const FVoxelSectionKey& K);
    WHFRAMEWORK_API FIntVector Local(const FIntVector& P);
    WHFRAMEWORK_API uint16 Linear(const FIntVector& L);
    WHFRAMEWORK_API FIntVector Unlinear(uint16 I);
    WHFRAMEWORK_API FIntVector Direction(uint8 Face);
    WHFRAMEWORK_API FVoxelSectionKey Neighbor(const FVoxelSectionKey& K,uint8 Face);
    WHFRAMEWORK_API bool FromWorld(const FVector& P,double BlockSize,FIntVector& Out);
    WHFRAMEWORK_API FVector ToWorld(const FIntVector& P,double BlockSize);
    WHFRAMEWORK_API bool IsValidSection(const FVoxelSectionKey& K,int32 MinZ,int32 MaxZ);
}
