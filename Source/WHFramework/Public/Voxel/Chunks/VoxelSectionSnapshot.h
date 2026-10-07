#pragma once

#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/Runtime/VoxelSection.h"

struct WHFRAMEWORK_API FVoxelSectionSnapshot
{
	FVoxelSectionSnapshot() = default;
	FVoxelSectionSnapshot(const FVoxelSectionSnapshot&) = delete;
	FVoxelSectionSnapshot& operator=(const FVoxelSectionSnapshot&) = delete;
	FVoxelSectionSnapshot(FVoxelSectionSnapshot&&) = default;
	FVoxelSectionSnapshot& operator=(FVoxelSectionSnapshot&& InOther);

	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FIntVector Section = FIntVector::ZeroValue;
	FVoxelSectionStamp Stamp;
	uint64 Revision = 0;
	int32 GridSide = 16;
	TArray<uint32> Blocks;
	TArray<uint32> Halo[6];
	bool Known[6] = { false, false, false, false, false, false };

	bool TrySample(const FIntVector& InLocal, FVoxelBlockState& OutState) const;
	uint64 Bytes() const;
};
