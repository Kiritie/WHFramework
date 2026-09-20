#pragma once

#include "CoreMinimal.h"
#include "Voxel/Streaming/VoxelInterest.h"

class UVoxelModule;
class UVoxelCollisionComponent;
class FVoxelTaskScheduler;
struct FVoxelTaskResult;

class WHFRAMEWORK_API FVoxelCollisionPresenter
{
public:
	FVoxelCollisionPresenter(
		UVoxelModule& InModule,
		FVoxelTaskScheduler& InScheduler,
		uint64 InWorldEpoch);

	void Tick(const TMap<FIntVector, FVoxelExactDemand>& InDemand);
	bool OnTask(FVoxelTaskResult&& InResult);
	void InvalidateSection(const FIntVector& InSection);
	bool IsReady(const FIntVector& InSection) const;
	void Reset();

private:
	void RequestCollision(const FIntVector& InSection, uint64 InRevision);

private:
	UVoxelModule& Module;
	FVoxelTaskScheduler& Scheduler;
	uint64 WorldEpoch = 0;
	TMap<FIntVector, TObjectPtr<UVoxelCollisionComponent>> SectionComponents;
	TMap<FIntVector, uint64> PublishedRevisions;
};
