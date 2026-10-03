#pragma once

#include "CoreMinimal.h"

struct FVoxelStreamingSource;

namespace VoxelViewLod
{
	WHFRAMEWORK_API double TexturePeriodCells(int32 InStep, double InMaximumStretchCells);

	WHFRAMEWORK_API uint8 ResolveVoxelProxyLevel(
		int32 InDistanceCells,
		const FVoxelStreamingSource& InSource,
		uint8 InMaximumLevel,
		float InTargetScreenErrorPixels);

	WHFRAMEWORK_API uint8 ResolveScreenErrorLevel(
		int32 InDistanceCells,
		int32 InBaseSampleStepCells,
		const FVoxelStreamingSource& InSource,
		uint8 InMaximumLevel,
		float InTargetScreenErrorPixels);
}
