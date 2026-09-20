#pragma once

#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

class WHFRAMEWORK_API FVoxelResidencyManager
{
public:
	FVoxelResidencyManager(
		FVoxelWorldRuntime& InRuntime,
		FVoxelTaskScheduler& InScheduler,
		TFunction<void(const FIntVector&)> InBeforeEvict);

	void Tick(
		const TMap<FIntVector, FVoxelExactDemand>& InDemand,
		uint64 InInterestRevision,
		double InNow);
	void SetEvictGraceFrames(uint64 InFrames);

private:
	bool CanEvict(
		const FIntVector& InKey,
		const FVoxelSection& InSection,
		uint64 InFrame) const;

private:
	static constexpr uint64 EvictionCheckIntervalFrames = 15;

	FVoxelWorldRuntime& Runtime;
	FVoxelTaskScheduler& Scheduler;
	TFunction<void(const FIntVector&)> BeforeEvict;
	TSet<FIntVector> Demanded;
	uint64 CurrentInterestRevision = 0;
	uint64 LastEvictionCheckFrame = 0;
	uint64 EvictGraceFrames = 180;
};
