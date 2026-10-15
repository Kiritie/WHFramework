#pragma once

#include "CoreMinimal.h"

enum class EVoxelGenerationPlanKind : uint8
{
	Hydrology,
	Cave,
	Structure,
	Feature,
	Ecology
};

struct WHFRAMEWORK_API FVoxelPlanDiagnostics
{
	uint64 BuildCount = 0;
	uint64 HitCount = 0;
	uint64 MissCount = 0;
	uint64 WaitingConsumers = 0;
	uint64 PriorityDonations = 0;
	uint64 FailedCount = 0;
	double TotalBuildMilliseconds = 0.0;
	double MaximumBuildMilliseconds = 0.0;

	void RecordBuild(const double InMilliseconds, const bool bInFailed)
	{
		++BuildCount;
		FailedCount += bInFailed ? 1 : 0;
		TotalBuildMilliseconds += InMilliseconds;
		MaximumBuildMilliseconds = FMath::Max(MaximumBuildMilliseconds, InMilliseconds);
	}
};
