#pragma once

class FVoxelTerrainResourceLease;

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"

struct WHFRAMEWORK_API FVoxelBaseColumnEntry
{
	// 原正文 grant 的共享来源；深层 backing 先析构，controller deleter 仍保活到最后 weak。
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FIntPoint Position = FIntPoint::ZeroValue;
	FVoxelColumnSample Column;

	uint64 GetAllocatedBytes() const
	{
		return sizeof(FVoxelBaseColumnEntry);
	}
};

struct WHFRAMEWORK_API FVoxelNaturalColumnEntry
{
	// 原正文 grant 的共享来源；深层 backing 先析构，controller deleter 仍保活到最后 weak。
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FIntPoint Position = FIntPoint::ZeroValue;
	FVoxelColumnSample Column;

	uint64 GetAllocatedBytes() const
	{
		return sizeof(FVoxelNaturalColumnEntry);
	}
};
