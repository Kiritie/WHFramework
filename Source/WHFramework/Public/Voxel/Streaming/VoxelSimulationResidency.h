#pragma once

#include "CoreMinimal.h"

struct WHFRAMEWORK_API FVoxelSimulationResidencyDelta
{
	TArray<FIntVector> AddedSections;
	TArray<FIntVector> RemovedSections;
	uint64 Revision = 0;
};
