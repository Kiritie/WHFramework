#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"

#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Rendering/VoxelViewTypes.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/Runtime/VoxelOverlaySnapshot.h"

class FVoxelGenerationPipeline;

struct WHFRAMEWORK_API FVoxelVoxelProxyData
{
	FVoxelViewKey Key;
	FVoxelTerrainGeometry Geometry;
	uint64 RequestId = 1;
	uint64 EstimatedMeshBytes = 0;

	uint64 Revision = 0;

	int32 GridSide = 16;
	FIntVector LocalMinCells = FIntVector::ZeroValue;
	FVoxelGenerationBounds GetBuildBounds() const
	{
		const FIntVector Min = Key.GetBounds().Min + LocalMinCells;
		return {Min, Min + FIntVector(GridSide * Geometry.SampleStepCells)};
	}

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
			ESPMode::ThreadSafe> InCache,
		TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator = nullptr);

	bool BuildNatural(
		const FVoxelViewKey& InKey,
		FVoxelVoxelProxyData& OutData,
		FString& OutError,
		const TAtomic<bool>* InCancel,
		const FVoxelTerrainGeometry& InGeometry,
		const FVoxelTerrainBuildShard* InShard = nullptr) const;

	bool ApplyTreeSilhouettes(
		const FVoxelViewKey& InKey,
		const FVoxelOverlaySnapshotSet& InOverlays,
		FVoxelVoxelProxyData& InOutData,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;
	bool Build(const FVoxelViewKey& InKey, const FVoxelOverlaySnapshotSet& InOverlays,
		FVoxelVoxelProxyData& OutData, FString& OutError, const TAtomic<bool>* InCancel,
		const FVoxelTerrainGeometry& InGeometry,
		const FVoxelTerrainBuildShard* InShard = nullptr) const;

private:
	TSharedRef<
		const FVoxelGenerationRuntimeConfig,
		ESPMode::ThreadSafe> Config;

	TSharedRef<
			FVoxelGenerationPlanCache,
			ESPMode::ThreadSafe> Cache;

	TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
};
