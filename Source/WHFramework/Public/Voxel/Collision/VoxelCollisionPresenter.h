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

	void Tick(
		const TMap<FIntVector, FVoxelExactDemand>& InDemand,
		uint64 InInterestRevision);
	bool OnTask(FVoxelTaskResult&& InResult);
	void InvalidateSection(const FIntVector& InSection);
	bool IsReady(const FIntVector& InSection) const;
	void Reset();

private:
	void RebuildWanted(
		const TMap<FIntVector, FVoxelExactDemand>& InDemand,
		uint64 InInterestRevision);
	void RequestCollision(
		const FIntVector& InSection,
		uint64 InRevision,
		const FVoxelExactDemand& InDemand);

private:
	UVoxelModule& Module;
	FVoxelTaskScheduler& Scheduler;
	uint64 WorldEpoch = 0;
	uint64 CurrentInterestRevision = 0;
	TMap<FIntVector, FVoxelExactDemand> Wanted;
	TMap<FIntVector, TObjectPtr<UVoxelCollisionComponent>> SectionComponents;
	TMap<FIntVector, uint64> PublishedRevisions;
};
