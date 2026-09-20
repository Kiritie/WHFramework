#pragma once

#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/Runtime/VoxelSection.h"

struct WHFRAMEWORK_API FVoxelSectionSnapshot
{
	FIntVector Section = FIntVector::ZeroValue;
	FVoxelSectionStamp Stamp;
	uint64 Revision = 0;
	TArray<uint32> Blocks;
	TArray<uint32> Halo[6];
	bool Known[6] = { false, false, false, false, false, false };

	bool TrySample(const FIntVector& InLocal, FVoxelBlockState& OutState) const;
	uint64 Bytes() const;
};
