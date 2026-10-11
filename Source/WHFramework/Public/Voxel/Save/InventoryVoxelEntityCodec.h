#pragma once

#include "CoreMinimal.h"
#include "Ability/Inventory/InventorySnapshotCodec.h"
#include "Voxel/Save/VoxelDeltaTypes.h"

// 库存实体只在 GT 解码为物品；Worker 仅验证有界字节信封。
class WHFRAMEWORK_API FInventoryVoxelEntityCodec
{
public:
    static constexpr uint16 Kind = 0x4901;
    static constexpr uint16 Schema = 2;
    static constexpr int32 MaximumBytes = FInventorySnapshotCodec::MaximumBytes + 16;

    static bool Register();
    static bool IsInventory(const FVoxelBlockEntityState& InState);
    static bool IsUninitialized(const FVoxelBlockEntityState& InState);
    static bool IsPublic(const FVoxelBlockEntityState& InState);
    static bool MakeDefault(FVoxelBlockEntityState& OutState, uint8 InVariant);
    static void MakePublic(FVoxelBlockEntityState& OutState);
    static TConstArrayView<uint8> GetPublicPayload();
    static bool Validate(const FVoxelBlockEntityState& InState);
    static bool Encode(const FInventorySnapshot& InSnapshot, const FInventoryItemRegistry& InRegistry,
        const TMap<FPrimaryAssetId, FInventoryProfileDefinition>& InProfiles, FVoxelBlockEntityState& OutState, FString& OutError);
    static bool Decode(const FVoxelBlockEntityState& InState, const FInventoryItemRegistry& InRegistry,
        const TMap<FPrimaryAssetId, FInventoryProfileDefinition>& InProfiles, FInventorySnapshot& OutSnapshot, FString& OutError);
};
