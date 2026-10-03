#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Generation/VoxelGenerationPlanCoordinator.h"
#include "Voxel/Generation/VoxelWorldManifest.h"
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
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache,
		const FVoxelRegionStore& InRegionStore,
		const FVoxelWorldManifest& InManifest,
		TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry);

	void Tick(
		const FVoxelInterestSet& InInterest,
		uint64 InInterestRevision,
		double InNow,
		const TMap<int32, double>& InDataAdmissionLimits,
		int32 InMaxBuildsPerFrame,
		double InAdmissionMilliseconds);

	bool OnTask(FVoxelTaskResult&& InResult);

	void SetRemoteChangeState(
		const FIntVector& InSection,
		EVoxelSectionChangeState InState);

	void Reset();

	FVoxelGenerationDependencyStatus QueryEnvironment(const FIntPoint& InCell, FVoxelEnvironmentSample& OutSample);
	FVoxelGenerationDependencyStatus EnsureEnvironmentBounds(const FVoxelGenerationBounds& InBounds,
		EVoxelWorkClass InWorkClass, int32 InSourcePriority, double InDistanceScore, double InForwardScore);
	uint64 GetWaitingPlanConsumers(EVoxelGenerationPlanKind InKind) const;

private:
	static EVoxelWorkClass ResolveWorkClass(const FVoxelExactDemand& InDemand);
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
	struct FWaitingPlanSection
	{
		TArray<FVoxelGenerationPlanDependency> Dependencies;
		TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Retention;
		double RetryAfterSeconds = 0.0;
	};

	struct FOverlayBoundsRequest
	{
		FVoxelGenerationBounds Bounds;
		int32 SourcePriority = 0;
	};

	FVoxelWorldRuntime& Runtime;
	FVoxelTaskScheduler& Scheduler;
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	TUniquePtr<FVoxelGenerationPlanCoordinator> PlanCoordinator;
	const FVoxelRegionStore& RegionStore;
	FVoxelWorldManifest Manifest;
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;

	TMap<FIntVector, FVoxelExactDemand> CurrentDemand;
	TMap<FIntVector, FWaitingPlanSection> WaitingPlanSections;
	TMap<FIntVector, EVoxelSectionChangeState> RemoteChangeStates;
	TArray<FIntVector> OrderedKeys;
	TArray<int32> AdmissionLanes[2];
	TArray<FOverlayBoundsRequest> OverlayBoundsRequests;
	TMap<FIntPoint, FVoxelGenerationDependencyStatus> EnvironmentColumnStates;

	uint64 CurrentInterestRevision = 0;
	int32 NextAdmissionIndices[2] = {};
	int32 NextOverlayBoundsIndex = 0;
	double OverlayBoundsRetryAfterSeconds = 0.0;
	double LastAdmissionDiagnosticTime = 0.0;
	int32 PlanPendingAttempts = 0;
	int32 OverlayPendingAttempts = 0;
	int32 PlanReadyAttempts = 0;
	int32 OverlayReadyAttempts = 0;
	int32 EnqueueAttempts = 0;
	int32 EnqueuedTasks = 0;
};
