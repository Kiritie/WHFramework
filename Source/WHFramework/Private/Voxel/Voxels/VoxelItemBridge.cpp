#include "Voxel/Voxels/VoxelItemBridge.h"

bool FVoxelItemBridge::ToItem(const FVoxelRegistrySnapshot& Registry, FVoxelBlockState State, int32 Count, FVoxelItem& Out)
{
    const FVoxelRuntimeDefinition* Definition = Registry.Find(State.TypeId);
    if (!Definition || State.IsAir() || !Registry.IsValid(State) || Count <= 0 || Count > 100000)
    {
        return false;
    }
    Out = FVoxelItem(Definition->AssetID, State.State, Count);
    return true;
}

bool FVoxelItemBridge::ToBlock(const FVoxelRegistrySnapshot& Registry, const FVoxelItem& Item, FVoxelBlockState& Out)
{
    if (!Item.IsValid())
    {
        return false;
    }
    const FVoxelRuntimeDefinition* Definition = Registry.Find(Item.VoxelAssetID);
    if (!Definition || Definition->TypeId == 0)
    {
        return false;
    }
    const FVoxelBlockState State{Definition->TypeId, static_cast<uint16>(Item.State)};
    if (!Registry.IsValid(State))
    {
        return false;
    }
    Out = State;
    return true;
}

bool FVoxelItemBridge::ToAbility(const FVoxelRegistrySnapshot& Registry, const FVoxelItem& Item, FAbilityItem& Out)
{
    FVoxelBlockState State;
    if (!ToBlock(Registry, Item, State))
    {
        return false;
    }
    // 放回背包后方向/连接/开关状态不持久化到通用 FAbilityItem；下次放置重新求值。
    Out = FAbilityItem(Item.VoxelAssetID, Item.Count, 0);
    return true;
}

bool FVoxelItemBridge::FromAbility(const FVoxelRegistrySnapshot& Registry, const FAbilityItem& Item, FVoxelItem& Out)
{
    if (!Item.ItemId.IsValid() || Item.Count <= 0 || Item.Level != 0 || Item.InstanceId.IsValid() || !Item.Properties.IsEmpty())
    {
        return false;
    }
    const FVoxelRuntimeDefinition* Definition = Registry.Find(Item.ItemId);
    if (!Definition || Definition->TypeId == 0)
    {
        return false;
    }
    Out = FVoxelItem(Item.ItemId, 0, Item.Count);
    return true;
}
