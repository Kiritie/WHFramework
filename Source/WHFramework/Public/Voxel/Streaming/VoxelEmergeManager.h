#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Generation/VoxelGenerationPlanCoordinator.h"
#include "Voxel/Generation/VoxelWorldManifest.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

class FVoxelRegionStore;
struct FVoxelExactBaseGenerationWork;
struct FVoxelExactOverlayDecodeWork;
struct FVoxelEnvironmentColumnWork;
struct FVoxelEnvironmentGeometryCapture;

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
		int32 InMaxBuildsPerFrame,
		double InAdmissionMilliseconds,
		const FVoxelTerrainFrameContext* InFrameContext = nullptr);

	bool OnTask(FVoxelTaskResult&& InResult);
	EVoxelTerrainResourceAdmission GetLastResourceAdmission() const { return LastResourceAdmission; }

	bool SetRemoteChangeState(const FIntVector& InSection, EVoxelSectionChangeState InState, FString& OutError);
	FVoxelGenerationConsumerKey GetControlConsumer() const { return GetAdmissionConsumer(); }
	EVoxelTerrainResourceAdmission GetLastRemoteChangeAdmission() const { return RemoteChangeAdmission.Admission; }
	// 只表示原数据消费者目录授予；Runtime 接纳/pin 仍以 AcquireDataDemand 返回值为准。
	EVoxelTerrainResourceAdmission GetLastDataDemandAdmission() const { return ExternalDataAdmission.Admission; }
	void SetRemoteOverlayResolver(TFunction<bool(const FIntVector&, uint64&,
		TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>&)> InResolver);

	void Reset();

	FVoxelGenerationDependencyStatus QueryEnvironment(const FIntPoint& InCell, FVoxelEnvironmentSample& OutSample);

	FVoxelGenerationDependencyStatus EnsureEnvironmentGeometry(const FVoxelGenerationConsumerKey& InConsumer,
		const FVoxelTerrainGeometry& InGeometry, EVoxelWorkClass InWorkClass,
		int32 InSourcePriority, double InDistanceScore, double InForwardScore);
	void VisitGenerationFactSources(const FVoxelGenerationConsumerKey& InConsumer, FVoxelGenerationRequiredSourceScope::FVisit InVisit) const;
	TSharedPtr<const FVoxelGenerationEnvironmentInputs, ESPMode::ThreadSafe> GetEnvironmentGeometryInputs(
		const FVoxelGenerationConsumerKey& InConsumer, const FVoxelTerrainGeometry& InGeometry) const;
	FVoxelGenerationDependencyStatus EnsureEnvironmentBounds(const FVoxelGenerationBounds& InBounds,
		EVoxelWorkClass InWorkClass, int32 InSourcePriority, double InDistanceScore, double InForwardScore);
	FVoxelGenerationDependencyStatus EnsureEnvironmentSamples(const FVoxelGenerationConsumerKey& InConsumer,
		const FIntPoint& InOrigin, int32 InWidth, int32 InHeight, int32 InStep, double InDistanceScore,
		bool& bOutNeedsAdvance);
	uint64 GetWaitingPlanConsumers(EVoxelGenerationPlanKind InKind) const;
	FVoxelGenerationDependencyStatus EnsureGenerationFacts(const FVoxelGenerationConsumerKey& InConsumer,
		const FVoxelGenerationBounds& InBounds, const FVoxelTerrainServicePriority& InPriority, bool bInAllowContinuation = false);
	void ReleaseGenerationFacts(const FVoxelGenerationConsumerKey& InConsumer);
	bool HasGenerationFactConsumer(const FVoxelGenerationConsumerKey& InConsumer) const;
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
	friend class FVoxelMapStyleEpochTest;
#endif
	static bool IsCriticalDemand(const FVoxelExactDemand& InDemand);
	void WakeCriticalSection(const FIntVector& InKey);
	void OnGenerationFactsReady(const FVoxelGenerationConsumerKey& InConsumer);
	static EVoxelWorkClass ResolveWorkClass(const FVoxelExactDemand& InDemand);
	bool RebuildDemand(const FVoxelInterestSet& InInterest, uint64 InInterestRevision, double InDeadline);
	FVoxelGenerationConsumerKey GetAdmissionConsumer() const;
	bool DeferTickAdmission(uint64 InBeforeAdmission, const FString& InError);
	bool ReserveAdmissionAges(int32 InCount, FString& OutError, uint64& OutBeforeAdmission);
	void ClearDemandRebuild();

	bool RequestSection(const FIntVector& InKey, const FVoxelExactDemand& InDemand);
	const FVoxelExactDemand* FindDemand(const FIntVector& InSection) const;
	bool ReserveExternalDataStorage(int32 InSectionCount, int32 InConsumerCount, FString& OutError);

	bool RequestBase(FVoxelSection& InSection, const FIntVector& InKey, const FVoxelExactDemand& InDemand);

	bool ResolveOverlay(FVoxelSection& InSection, const FIntVector& InKey, const FVoxelExactDemand& InDemand);
	bool ReserveFactStorage(int32 InWaiting, int32 InExternal, FString& OutError);

private:
	struct FWaitingPlanSection
	{
		FVoxelGenerationConsumerKey Consumer;
		FVoxelGenerationPlanDependencyCapture Dependencies;
		TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Retention;
		FVoxelSectionStamp Stamp;
		double BlockedSinceSeconds = 0.0;
		bool bPlanReady = false;
		bool bOverlayReady = false;
		double RetryAfterSeconds = 0.0;
		uint64 CapacityVersion = 0;
		bool bWaitingCapacity = false;
		TSharedPtr<FVoxelExactBaseGenerationWork, ESPMode::ThreadSafe> BaseWork;
		TSharedPtr<FVoxelExactOverlayDecodeWork, ESPMode::ThreadSafe> OverlayWork;
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> AdmissionResources;
		uint64 AdmissionCapacityVersion = 0;
		bool bWaitingTaskCapacity = false;
		bool bTaskSubmitted = false;
		TOptional<FVoxelTaskRequest> PendingRequest;
	};

	FWaitingPlanSection* FindOrCreateWaiting(FVoxelSection& InSection, const FIntVector& InKey);
	bool DeferExactAdmission(FVoxelSection& InSection, const FIntVector& InKey,
		FWaitingPlanSection& InWaiting, TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		uint64 InBeforeAdmission, const FString& InError);
	bool SubmitExactRequest(FVoxelSection& InSection, const FIntVector& InKey,
		FWaitingPlanSection& InWaiting, const FVoxelExactDemand& InDemand);

	FVoxelWorldRuntime& Runtime;
	FVoxelTaskScheduler& Scheduler;
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	TUniquePtr<FVoxelGenerationPlanCoordinator> PlanCoordinator;
	const FVoxelRegionStore& RegionStore;
	FVoxelWorldManifest Manifest;
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;

	struct FInputBudget;
	struct FStorageAdmission
	{
		EVoxelTerrainResourceAdmission Admission = EVoxelTerrainResourceAdmission::None;
		uint64 CapacityVersion = 0;
		bool bWaitingCapacity = false;
	};
	void VisitStorageSources(const FVoxelTerrainResources& InResources, FVoxelGenerationRequiredSourceScope::FVisit InVisit) const;
	bool CanRetryStorage(FStorageAdmission& InAdmission, const FVoxelTerrainResources& InResources,
		const TCHAR* InError, FString& OutError) const;
	struct FCriticalReadyQueue
	{
		TArray<FIntVector> Values;
		int32 Head = 0;
		int32 Count = 0;
		bool IsEmpty() const { return Count == 0; }
		int32 Num() const { return Count; }
		const FIntVector& At(int32 InIndex) const;
		bool Peek(FIntVector& OutKey) const;
		bool Dequeue(FIntVector& OutKey);
		bool Enqueue(const FIntVector& InKey);
		void Empty();
	};
	struct FDemandRebuild
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		TMap<FIntVector, FVoxelExactDemand> Demand;
		TArray<FIntVector> Order;
		TArray<int32> Lanes[2];
		TArray<FIntVector> Critical;
		FCriticalReadyQueue Ready;
		TSet<FIntVector> Queued;
		uint64 InterestRevision = 0;
		uint64 PriorityRevision = 0;
		int32 Phase = 0;
		int32 Cursor = 0;
		bool bActive = false;
	};
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> DemandStorageLease;
	TMap<FIntVector, FVoxelExactDemand> CurrentDemand;
	FDemandRebuild DemandRebuild;
	EVoxelTerrainResourceAdmission LastResourceAdmission = EVoxelTerrainResourceAdmission::None;
	uint64 TickCapacityVersion = 0;
	bool bTickWaitingCapacity = false;
	struct FExternalDataDemand
	{
		FVoxelExactDemand Demand;
		FVoxelRuntimeSectionIdentity Identity;
		int32 References = 0;
		int32 OrderIndex = INDEX_NONE;
	};
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ExternalDataStorageLease;
	TMap<FIntVector, FExternalDataDemand> ExternalDataDemands;
	TSparseArray<FIntVector> ExternalDataOrder;
	struct FExternalDataConsumer
	{
		FIntVector Section;
		FVoxelRuntimeSectionPin Pin;
	};
	TMap<FGuid, FExternalDataConsumer> ExternalDataConsumers;
	FStorageAdmission ExternalDataAdmission;
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
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> AdmissionAgeStorageLease;
	TMap<FIntVector, FAdmissionServiceAge> AdmissionAges;
	uint64 NextAdmissionSequence = 0;
	double AdmissionCostEstimateMicroseconds = 25.0;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> FactStorageLease;
	FStorageAdmission FactStorageAdmission;
	int32 WaitingFactCapacity = 0;
	int32 ExternalFactCapacity = 0;
	TMap<FIntVector, FWaitingPlanSection> WaitingPlanSections;
	struct FEnvironmentWait
	{
		uint64 WakeSerial = 0;
		EVoxelWorkClass WorkClass = EVoxelWorkClass::None;
		int32 SourcePriority = 0;
		double DistanceScore = 0.0;
		double ForwardScore = 0.0;
	};
	struct FExternalFactConsumer
	{
		FVoxelGenerationBounds Bounds;
		FVoxelGenerationPlanDependencyCapture Dependencies;
		TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Retention;
		TSharedPtr<const FVoxelGenerationEnvironmentInputs, ESPMode::ThreadSafe> EnvironmentInputs;
		TOptional<FEnvironmentWait> EnvironmentWait;
		TSharedPtr<FVoxelEnvironmentGeometryCapture, ESPMode::ThreadSafe> GeometryCapture;
		TOptional<FVoxelTaskRequest> PendingGeometryCapture;
		FString GeometryCaptureError;
		double GeometryCaptureQueuedAt = 0.0;
		bool bGeometryCaptureSubmitted = false;
		bool bGeometryCaptureAdmissionWaiting = false;
		uint64 GeometryCaptureAdmissionVersion = 0;
		TOptional<FVoxelTerrainServicePriority> ContinuationPriority;
		TOptional<FVoxelTerrainServicePriority> NaturalWaitPriority;
		uint64 NaturalWaitWakeSerial = 0;
		uint64 WakeSerial = 0;
		uint64 NaturalWakeSerial = 0;
		bool bNaturalReady = false;
		bool bNaturalNeedsAdvance = false;
		int32 SampleStep = 0;
		TOptional<FVoxelTerrainGeometry> SamplingGeometry;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SampleRetentionStorageLease;
		TArray<TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe>> SampleRetentions;
		int32 SampleDependencyCursor = 0;
		uint64 CapacityVersion = 0;
		bool bWaitingCapacity = false;
	};
	TMap<FVoxelGenerationConsumerKey, FExternalFactConsumer> ExternalFactConsumers;
	TFunction<void(const FVoxelGenerationConsumerKey&)> ExternalConsumerWakeCallback;
	FGuid GenerationOwnerId = FGuid::NewGuid();
	uint64 NextGenerationRequestSerial = 0;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RemoteChangeStorageLease;
	FStorageAdmission RemoteChangeAdmission;
	TMap<FIntVector, EVoxelSectionChangeState> RemoteChangeStates;
	TFunction<bool(const FIntVector&, uint64&,
		TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>&)> RemoteOverlayResolver;
	TArray<FIntVector> OrderedKeys;
	TArray<int32> AdmissionLanes[2];
	TArray<FIntVector> CriticalAdmissionKeys;
	FCriticalReadyQueue ReadyCriticalSections;
	TSet<FIntVector> QueuedCriticalSections;
	int32 CriticalFallbackCursor = 0;
	enum class EEnvironmentFailure : uint8
	{
		None = 0,
		Oversized,
		CapacityUnachievable,
		InvalidRequest,
		InvalidResource,
		InvalidConsumer,
		Failure
	};
	static EEnvironmentFailure ClassifyEnvironmentFailure(const FString& InError);
	static const TCHAR* GetEnvironmentFailureError(EEnvironmentFailure InFailure);
	struct FEnvironmentColumnAttempt
	{
		FVoxelTaskStamp Stamp;
		FVoxelGenerationConsumerKey Consumer;
		EEnvironmentFailure Failure = EEnvironmentFailure::None;
		TSharedPtr<FVoxelEnvironmentColumnWork, ESPMode::ThreadSafe> Work;
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> AdmissionResources;
		uint64 AdmissionCapacityVersion = 0;
		bool bWaitingCapacity = false;
		bool bSubmitted = false;
		TOptional<FVoxelTaskRequest> PendingRequest;
	};
	FVoxelGenerationDependencyStatus AdvanceEnvironmentGeometryCapture(const FVoxelGenerationConsumerKey& InConsumer, EVoxelWorkClass InWorkClass,
		int32 InSourcePriority, double InDistanceScore, double InForwardScore);
	bool ReserveEnvironmentStorage(FString& OutError);
	FVoxelGenerationDependencyStatus DeferEnvironmentAdmission(const FIntPoint& InCell,
		FEnvironmentColumnAttempt& InAttempt, TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		uint64 InBeforeAdmission, const FString& InError);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> EnvironmentStorageLease;
	TMap<FIntPoint, FEnvironmentColumnAttempt> EnvironmentColumnStates;
	FGuid EnvironmentTaskOwnerId = FGuid::NewGuid();
	uint64 EnvironmentTaskIncarnation = 1;
	uint64 NextEnvironmentTaskSerial = 0;
	TSharedPtr<bool> EnvironmentCaptureLifetime = MakeShared<bool>(true);

	bool bResetting = false;
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
