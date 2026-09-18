#include "Voxel/Voxels/VoxelItemBridge.h"
bool FVoxelItemBridge::ToItem(const FVoxelRegistrySnapshot&R,FVoxelBlockState S,int32 N,FVoxelItem&O)
{
    const auto*D=R.Find(S.TypeId);if(!D||S.IsAir()||!R.IsValid(S)||N<=0||N>100000)return false;
    O=FVoxelItem(D->AssetID,S.State,N);return true;
}
bool FVoxelItemBridge::ToBlock(const FVoxelRegistrySnapshot&R,const FVoxelItem&I,FVoxelBlockState&O)
{
    if(!I.IsValid())return false;const auto*D=R.Find(I.VoxelAssetID);if(!D||D->TypeId==0)return false;
    FVoxelBlockState S{D->TypeId,uint16(I.State)};if(!R.IsValid(S))return false;O=S;return true;
}
bool FVoxelItemBridge::ToAbility(const FVoxelRegistrySnapshot&R,const FVoxelItem&I,FAbilityItem&O)
{
    FVoxelBlockState S;if(!ToBlock(R,I,S))return false;
    // 放回背包后方向/连接/开关状态不持久化到通用 FAbilityItem；下次放置重新求值。
    O=FAbilityItem(I.VoxelAssetID,I.Count,0);return true;
}
bool FVoxelItemBridge::FromAbility(const FVoxelRegistrySnapshot&R,const FAbilityItem&I,FVoxelItem&O)
{
    if(!I.IsValid()||I.Count<=0||I.Level!=0)return false;const auto*D=R.Find(I.ID);if(!D||D->TypeId==0)return false;
    O=FVoxelItem(I.ID,0,I.Count);return true;
}
