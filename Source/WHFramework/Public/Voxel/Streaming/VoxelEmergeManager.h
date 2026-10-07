#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
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

	~FVoxelEmergeManager();

	void Tick(
		const FVoxelInterestSet& InInterest,
		uint64 InInterestRevision,
		double InNow,
		const TMap<int32, double>& InDataAdmissionLimits,
		int32 InMaxBuildsPerFrame,
		double InAdmissionMilliseconds,
		const FVoxelTerrainFrameContext* InFrameContext = nullptr);

	bool OnTask(FVoxelTaskResult&& InResult);

	void SetRemoteChangeState(
		const FIntVector& InSection,
		EVoxelSectionChangeState InState);
	void SetRemoteOverlayResolver(TFunction<bool(const FIntVector&, uint64&,
		TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>&)> InResolver);

	void Reset();

	FVoxelGenerationDependencyStatus QueryEnvironment(const FIntPoint& InCell, FVoxelEnvironmentSample& OutSample);
	FVoxelGenerationDependencyStatus EnsureEnvironmentBounds(const FVoxelGenerationBounds& InBounds,
		EVoxelWorkClass InWorkClass, int32 InSourcePriority, double InDistanceScore, double InForwardScore);
	uint64 GetWaitingPlanConsumers(EVoxelGenerationPlanKind InKind) const;
	FVoxelGenerationDependencyStatus EnsureGenerationFacts(const FVoxelGenerationConsumerKey& InConsumer,
		const FVoxelGenerationBounds& InBounds, const FVoxelTerrainServicePriority& InPriority);
	void ReleaseGenerationFacts(const FVoxelGenerationConsumerKey& InConsumer);
	void SetExternalConsumerWakeCallback(TFunction<void(const FVoxelGenerationConsumerKey&)> InCallback);
	// 非视觉 Runtime 数据消费者共用原接纳/生成/Overlay 主链，并持有稳定实例 pin。
	bool AcquireDataDemand(
		const FIntVector& InSection,
		FVoxelRuntimeSectionPin& OutPin);
	bool ReleaseDataDemand(
		const FIntVector& InSection,
		const FVoxelRuntimeSectionPin& InPin);

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FVoxelCriticalReadyQueueLivenessTest;
	friend class FVoxelEmergeCapacityContinuationTest;
#endif
	static bool IsCriticalDemand(const FVoxelExactDemand& InDemand);
	void WakeCriticalSection(const FIntVector& InKey);
	void OnGenerationFactsReady(const FVoxelGenerationConsumerKey& InConsumer);
	static EVoxelWorkClass ResolveWorkClass(const FVoxelExactDemand& InDemand);
	void RebuildDemand(const FVoxelInterestSet& InInterest, uint64 InInterestRevision);

	bool RequestSection(const FIntVector& InKey, const FVoxelExactDemand& InDemand);
	const FVoxelExactDemand* FindDemand(const FIntVector& InSection) const;
	bool ReserveExternalDataStorage(
		int32 InSectionCount,
		int32 InConsumerCount);

	bool RequestBase(FVoxelSection& InSection, const FIntVector& InKey, const FVoxelExactDemand& InDemand);

	bool ResolveOverlay(FVoxelSection& InSection, const FIntVector& InKey, const FVoxelExactDemand& InDemand);

private:
	struct FWaitingPlanSection
	{
		FVoxelGenerationConsumerKey Consumer;
		TArray<FVoxelGenerationPlanDependency> Dependencies;
		TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Retention;
		FVoxelSectionStamp Stamp;
		double BlockedSinceSeconds = 0.0;
		FString Blocker;
		double RetryAfterSeconds = 0.0;
	};

	FVoxelWorldRuntime& Runtime;
	FVoxelTaskScheduler& Scheduler;
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	TUniquePtr<FVoxelGenerationPlanCoordinator> PlanCoordinator;
	const FVoxelRegionStore& RegionStore;
	FVoxelWorldManifest Manifest;
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;

	TMap<FIntVector, FVoxelExactDemand> CurrentDemand;
	struct FExternalDataDemand
	{
		FVoxelExactDemand Demand;
		FVoxelRuntimeSectionIdentity Identity;
		int32 References = 0;
		int32 OrderIndex = INDEX_NONE;
	};
	TMap<FIntVector, FExternalDataDemand> ExternalDataDemands;
	TSparseArray<FIntVector> ExternalDataOrder;
	struct FExternalDataConsumer
	{
		FIntVector Section;
		FVoxelRuntimeSectionPin Pin;
	};
	TMap<FGuid, FExternalDataConsumer> ExternalDataConsumers;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ExternalDataStorageLease;
	int32 ExternalDataCapacity = 0;
	int32 ExternalDataConsumerCapacity = 0;
	int32 ExternalDataCursor = 0;
	struct FAdmissionServiceAge
	{
		double FirstQueuedSeconds = 0.0;
		uint64 Sequence = 0;
		uint64 SectionToken = 0;
	};
	FVoxelTerrainWorkSelection AdmissionSelection;
	TMap<FIntVector, FAdmissionServiceAge> AdmissionAges;
	uint64 NextAdmissionSequence = 0;
	double AdmissionCostEstimateMicroseconds = 25.0;
	TMap<FIntVector, FWaitingPlanSection> WaitingPlanSections;
	struct FExternalFactConsumer
	{
		FVoxelGenerationBounds Bounds;
		TArray<FVoxelGenerationPlanDependency> Dependencies;
		TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Retention;
	};
	TMap<FVoxelGenerationConsumerKey, FExternalFactConsumer> ExternalFactConsumers;
	TFunction<void(const FVoxelGenerationConsumerKey&)> ExternalConsumerWakeCallback;
	FGuid GenerationOwnerId = FGuid::NewGuid();
	uint64 NextGenerationRequestSerial = 0;
	TMap<FIntVector, EVoxelSectionChangeState> RemoteChangeStates;
	TFunction<bool(const FIntVector&, uint64&,
		TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>&)> RemoteOverlayResolver;
	TArray<FIntVector> OrderedKeys;
	TArray<int32> AdmissionLanes[2];
	TArray<FIntVector> CriticalAdmissionKeys;
	TQueue<FIntVector> ReadyCriticalSections;
	TSet<FIntVector> QueuedCriticalSections;
	int32 CriticalFallbackCursor = 0;
	struct FEnvironmentColumnAttempt
	{
		FVoxelTaskStamp Stamp;
		FVoxelGenerationDependencyStatus Status;
	};
	TMap<FIntPoint, FEnvironmentColumnAttempt> EnvironmentColumnStates;
	FGuid EnvironmentTaskOwnerId = FGuid::NewGuid();
	uint64 EnvironmentTaskIncarnation = 1;
	uint64 NextEnvironmentTaskSerial = 0;

	uint64 CurrentInterestRevision = 0;
	uint64 CurrentPriorityRevision = 0;
	int32 NextAdmissionIndices[2] = {};
	double LastAdmissionDiagnosticTime = 0.0;
	int32 PlanPendingAttempts = 0;
	int32 OverlayPendingAttempts = 0;
	int32 PlanReadyAttempts = 0;
	int32 OverlayReadyAttempts = 0;
	int32 EnqueueAttempts = 0;
	int32 EnqueuedTasks = 0;
};
