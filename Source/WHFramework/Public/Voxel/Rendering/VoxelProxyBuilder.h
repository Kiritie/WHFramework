#pragma once

#include "CoreMinimal.h"

#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Rendering/VoxelViewTypes.h"
#include "Voxel/Runtime/VoxelBlockState.h"

struct WHFRAMEWORK_API FVoxelVoxelProxyData
{
	FVoxelViewKey Key;

	uint64 Revision = 0;

	int32 GridSide = 16;

	bool bHasVisibleSurfaceEvidence =
		false;

	TArray<FVoxelBlockState> Cells;

	/**
	 * 与 FVoxelSectionSnapshot 面序一致：
	 *
	 * 0 = +X
	 * 1 = -X
	 * 2 = +Y
	 * 3 = -Y
	 * 4 = +Z
	 * 5 = -Z
	 */
	TArray<FVoxelBlockState> Halo[6];

	bool Known[6] = {
		false,
		false,
		false,
		false,
		false,
		false
	};

	uint64 GetAllocatedBytes() const;
};

class WHFRAMEWORK_API FVoxelVoxelProxyBuilder
{
public:
	FVoxelVoxelProxyBuilder(
		TSharedRef<
			const FVoxelGenerationRuntimeConfig,
			ESPMode::ThreadSafe> InConfig,
		TSharedRef<
			FVoxelGenerationPlanCache,
			ESPMode::ThreadSafe> InCache);

	bool BuildNatural(
		const FVoxelViewKey& InKey,
		FVoxelVoxelProxyData& OutData,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

private:
	TSharedRef<
		const FVoxelGenerationRuntimeConfig,
		ESPMode::ThreadSafe> Config;

	TSharedRef<
		FVoxelGenerationPlanCache,
		ESPMode::ThreadSafe> Cache;
};
