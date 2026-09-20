#pragma once

#include "CoreMinimal.h"

struct FVoxelStreamingSource;
struct FVoxelViewSettings;

namespace VoxelViewLod
{
	WHFRAMEWORK_API uint8 ResolveScreenErrorLevel(
		int32 InDistanceCells,
		int32 InBaseSampleStepCells,
		const FVoxelStreamingSource& InSource,
		const FVoxelViewSettings& InSettings,
		uint8 InMaximumLevel);
}
