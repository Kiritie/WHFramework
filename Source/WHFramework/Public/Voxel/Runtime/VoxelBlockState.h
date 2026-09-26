#pragma once
#include "CoreMinimal.h"
using FVoxelTypeId = uint16;
namespace VoxelBlock
{
    constexpr uint16 Air = 0;
    constexpr int32 Size = 16;
    constexpr int32 Volume = 4096;
    constexpr int32 MaxAbsCoordinate = 1 << 24;
}
struct WHFRAMEWORK_API FVoxelBlockState
{
    uint16 TypeId = 0;
    uint16 State = 0;
    bool IsAir() const { return TypeId == VoxelBlock::Air; }
    uint32 Pack() const { return uint32(TypeId) | (uint32(State) << 16); }
    static FVoxelBlockState Unpack(uint32 V) { return {uint16(V), uint16(V >> 16)}; }
    bool operator==(const FVoxelBlockState& B) const { return Pack() == B.Pack(); }
    bool operator!=(const FVoxelBlockState& B) const { return !(*this == B); }
};
static_assert(sizeof(FVoxelBlockState)==4, "World cell must remain four bytes");
namespace VoxelState
{
    constexpr uint16 FacingMask=7, HalfMask=8, OpenMask=16, HingeMask=32;
    constexpr uint16 WaterloggedMask=64, VariantMask=0x0780;
    constexpr uint16 ExtraMask=0xf800;
    WHFRAMEWORK_API uint8 GetFacing(uint16 S);
    WHFRAMEWORK_API uint16 WithFacing(uint16 S, uint8 Facing);
    WHFRAMEWORK_API uint16 WithFlag(uint16 S, uint16 Mask, bool bSet);
    WHFRAMEWORK_API uint8 GetConnections(uint16 S);
    WHFRAMEWORK_API uint16 WithConnections(uint16 S, uint8 Mask);
}
