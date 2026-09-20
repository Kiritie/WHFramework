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
	bool bHasVisibleSurfaceEvidence = false;
	TArray<FVoxelBlockState> Cells;
};

class WHFRAMEWORK_API FVoxelVoxelProxyBuilder
{
public:
	FVoxelVoxelProxyBuilder(
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache);

	bool BuildNatural(
		const FVoxelViewKey& InKey,
		FVoxelVoxelProxyData& OutData,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

private:
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
};
