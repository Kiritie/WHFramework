#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

class FVoxelRegionStore;

enum class EVoxelSectionChangeState : uint8
{
	Unknown = 0,
	Natural,
	Modified
};

class WHFRAMEWORK_API FVoxelEmergeManager
{
public:
	FVoxelEmergeManager(
		FVoxelWorldRuntime& InRuntime,
		FVoxelTaskScheduler& InScheduler,
		TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
		const FVoxelRegionStore& InRegionStore,
		const FVoxelWorldManifest& InManifest,
		TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry);

	void Tick(
		const FVoxelInterestSet& InInterest,
		uint64 InInterestRevision,
		double InNow,
		double InDataAdmissionLimit,
		int32 InMaxBuildsPerFrame,
		double InAdmissionMilliseconds);

	bool OnTask(FVoxelTaskResult&& InResult);

	void SetRemoteChangeState(
		const FIntVector& InSection,
		EVoxelSectionChangeState InState);

	void Reset();

private:
	void RebuildDemand(
		const FVoxelInterestSet& InInterest,
		uint64 InInterestRevision);

	bool RequestSection(
		const FIntVector& InKey,
		const FVoxelExactDemand& InDemand);

	bool RequestBase(
		FVoxelSection& InSection,
		const FIntVector& InKey,
		const FVoxelExactDemand& InDemand);

	bool ResolveOverlay(
		FVoxelSection& InSection,
		const FIntVector& InKey,
		const FVoxelExactDemand& InDemand);

private:
	FVoxelWorldRuntime& Runtime;
	FVoxelTaskScheduler& Scheduler;
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	const FVoxelRegionStore& RegionStore;
	FVoxelWorldManifest Manifest;
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;

	TMap<FIntVector, FVoxelExactDemand> CurrentDemand;
	TMap<FIntVector, EVoxelSectionChangeState> RemoteChangeStates;
	TArray<FIntVector> OrderedKeys;

	uint64 CurrentInterestRevision = 0;
	int32 NextAdmissionIndex = 0;
};
