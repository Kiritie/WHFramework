#pragma once

#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/Save/VoxelDeltaTypes.h"

struct FVoxelSection;

namespace VoxelRuntimeStorage
{
	// 每代最终数据同时预留这个固定退休头，替换时只移动容器，不复制或在 GT 深度析构。
	struct FRetiredData
	{
		TArray<FVoxelBlockState> Blocks;
		TMap<int32, FVoxelBlockState> Overlay;
		TMap<int32, FVoxelBlockEntityState> Entities;
	};

	struct FRetiredStorage
	{
		TMap<FIntVector, TSharedPtr<FVoxelSection, ESPMode::ThreadSafe>> Sections;
		TSparseArray<FIntVector> ResidencyOrder;
		TArray<FVoxelSection*> RetirementTimers;
		TSet<FIntVector> ResidentPromotion;
	};
}
