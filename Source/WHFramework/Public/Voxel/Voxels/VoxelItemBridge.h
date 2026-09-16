#pragma once
#include "Ability/AbilityModuleTypes.h"
#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/VoxelModuleTypes.h"
class WHFRAMEWORK_API FVoxelItemBridge
{
public:
	static bool ToItem(const FVoxelRegistrySnapshot& Registry, FVoxelBlockState State, int32 Count, FVoxelItem& Out);
	static bool ToBlock(const FVoxelRegistrySnapshot& Registry, const FVoxelItem& Item, FVoxelBlockState& Out);
	static bool ToAbility(const FVoxelRegistrySnapshot& Registry, const FVoxelItem& Item, FAbilityItem& Out);
	static bool FromAbility(const FVoxelRegistrySnapshot& Registry, const FAbilityItem& Item, FVoxelItem& Out);
};
