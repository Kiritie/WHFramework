#include "Voxel/Runtime/VoxelBlockState.h"
uint8 VoxelState::GetFacing(uint16 S) { return uint8(S & FacingMask); }
uint16 VoxelState::WithFacing(uint16 S, uint8 F)
{
    check(F<6); return uint16((S & ~FacingMask) | F);
}
uint16 VoxelState::WithFlag(uint16 S,uint16 M,bool B)
{
    return B ? uint16(S|M) : uint16(S&~M);
}
uint8 VoxelState::GetConnections(uint16 S) { return uint8((S>>11)&15); }
uint16 VoxelState::WithConnections(uint16 S,uint8 M)
{
    return uint16((S&~uint16(15u<<11)) | (uint16(M&15)<<11));
}
