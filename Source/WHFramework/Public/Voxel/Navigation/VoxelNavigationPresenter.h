#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Voxel/Navigation/VoxelNavigationTypes.h"
#include "Voxel/Streaming/VoxelInterest.h"

class UVoxelModule;
class FVoxelTaskScheduler;
class UVoxelNavigationSurfaceComponent;
class AVoxelNavigationBounds;
struct FVoxelTaskResult;

class WHFRAMEWORK_API FVoxelNavigationPresenter
{
public:
	FVoxelNavigationPresenter(UVoxelModule& InModule, FVoxelTaskScheduler& InScheduler);
	~FVoxelNavigationPresenter();
	void UpdateSources(const FVoxelInterestSet& InInterest);
	void ActivateSection(const FIntVector& InSection);
	void RetireSection(const FIntVector& InSection);
	void InvalidateSection(const FIntVector& InSection);
	void Tick();
	bool IsReady(const FIntVector& InSection) const;
	void Reset();

private:
	bool RequestSection(const FIntVector& InSection);
	void MarkDirty(const FIntVector& InSection);
	void Apply(FVoxelTaskResult&& InResult);

	UVoxelModule& Module;
	FVoxelTaskScheduler& Scheduler;
	uint64 WorldEpoch = 0;
	TSet<FIntVector> Wanted;
	TSet<FIntVector> Dirty;
	TQueue<FIntVector> PendingSections;
	TMap<FIntVector, uint64> PublishedRevisions;
	TMap<FIntVector, TObjectPtr<UVoxelNavigationSurfaceComponent>> Components;
	TMap<FGuid, TWeakObjectPtr<AVoxelNavigationBounds>> SourceBounds;
};
