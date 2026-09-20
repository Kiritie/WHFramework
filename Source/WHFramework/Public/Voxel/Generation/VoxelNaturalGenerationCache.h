#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"

struct WHFRAMEWORK_API FVoxelBaseColumnEntry
{
	FIntPoint Position = FIntPoint::ZeroValue;
	FVoxelColumnSample Column;

	uint64 GetAllocatedBytes() const
	{
		return sizeof(FVoxelBaseColumnEntry);
	}
};

struct WHFRAMEWORK_API FVoxelNaturalColumnEntry
{
	FIntPoint Position = FIntPoint::ZeroValue;
	FVoxelColumnSample Column;

	uint64 GetAllocatedBytes() const
	{
		return sizeof(FVoxelNaturalColumnEntry);
	}
};
