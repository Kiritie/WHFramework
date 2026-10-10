#pragma once

#include "CoreMinimal.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Rendering/VoxelCoverage.h"
#include "Voxel/Rendering/VoxelCoverageIndex.h"
#include "Voxel/Rendering/VoxelPublishBudget.h"
#include "Voxel/Rendering/VoxelFinePage.h"
#include "Voxel/Rendering/VoxelPublishGroups.h"
#include "Voxel/Rendering/DWLodTransition.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Rendering/VoxelFineBuildState.h"
#include "Voxel/Runtime/VoxelChangeHierarchy.h"
#include "Voxel/Runtime/VoxelFineOverlayRead.h"

struct FVoxelHeightfieldCoveragePlan;
struct FVoxelVolumeCoveragePlan;
class AActor;
class FVoxelTaskScheduler;
class FVoxelGenerationPipeline;
class FVoxelGenerationCacheRetentionLease;
struct FVoxelGenerationBounds;
class FVoxelViewPublisher;
class UVoxelMeshComponent;
class UVoxelModule;

struct FVoxelMacroTileData;
struct FVoxelCoverageBox;
struct FVoxelCoverageRect;
struct FVoxelRepresentationInvalidate;
struct FVoxelRepresentationReply;
struct FVoxelSectionMeshResult;
struct FVoxelSectionSnapshot;
struct FVoxelSectionStamp;
struct FVoxelTaskResult;
struct FVoxelInterestDelta;
struct FVoxelVoxelProxyData;
struct FVoxelWaterSurfaceTileData;
struct FVoxelBoundaryTransitionContext;
struct FVoxelFrozenHeightfieldInputs;
struct FVoxelAffectedRepresentationKeys;
struct FVoxelFineShardGeometry;
struct FVoxelTerrainRenderAtom;
class IVoxelTerrainPresentedSnapshot;

struct WHFRAMEWORK_API FVoxelTerrainGeometryMetrics
{
	FVoxelTerrainGeometry Geometry;
	uint64 Vertices = 0;
	uint64 Triangles = 0;
	uint64 MeshBytes = 0;
	double BuildMilliseconds = 0.0;
	bool bGenerated = false;
	bool bPresented = false;
};

struct WHFRAMEWORK_API FVoxelTerrainWorkMetrics
{
	uint64 Completed = 0;
	uint64 Failed = 0;
	uint64 Canceled = 0;
	uint64 Stale = 0;
	uint64 Retried = 0;
	double BuildMilliseconds = 0.0;
};

struct WHFRAMEWORK_API FVoxelTerrainTargetReadiness
{
	FVoxelTerrainGeometry Geometry;
	bool bDataReady = false;
	bool bMeshReady = false;
	bool bPresented = false;
	bool bVisible = false;
	bool bKnownEmpty = false;
	bool bFailed = false;
	bool bDirty = false;
	bool bPublishing = false;
	int32 RequiredSections = 0;
	int32 ReadySections = 0;
	int32 PresentedSections = 0;
};

struct WHFRAMEWORK_API FVoxelTerrainReadinessSnapshot
{
	uint64 WorldEpoch = 0;
	uint64 PublicationRevision = 0;
	uint64 FineLayoutRevision = 0;
	bool bAdoptingInputs = false;
	TMap<FGuid, FVoxelStreamingSource> Sources;
	TArray<FVoxelTerrainTargetReadiness> Targets;
};

struct WHFRAMEWORK_API FVoxelPrimaryFineReadiness
{
	int32 Required = 0;
	int32 Ready = 0;
	int32 Renderable = 0;
	int32 Presented = 0;
	int32 Owned = 0;
	int32 Hidden = 0;
	int32 PublicationPending = 0;
	bool bFailed = false;

	bool IsComplete() const
	{
		return Required > 0 && Ready >= Required && Presented >= Required;
	}
};

class WHFRAMEWORK_API FVoxelViewManager
{
public:
	FVoxelViewManager(
		UVoxelModule& InModule,
		FVoxelTaskScheduler& InScheduler,
		uint64 InWorldEpoch);

	~FVoxelViewManager();

	void Tick(
		uint64 InInterestRevision,
		TConstArrayView<FVector> InObservers);
	void RefreshTaskPriorities();
	void AdvanceInterestInputs(uint64 InRevision, TConstArrayView<FVector> InObservers);
	void ApplyInterestDelta(const FVoxelInterestDelta& InDelta);
	FVoxelFrameTimings GetFrameTimings() const { return FrameTimings; }
	EVoxelTerrainResourceAdmission GetLastServiceResourceAdmission() const { return LastServiceResourceAdmission; }
	FVoxelGenerationConsumerKey GetServiceControlConsumer() const
	{
		return FVoxelGenerationConsumerKey::Section(WorldEpoch, TaskOwnerId, TaskOwnerIncarnation, {}, 1);
	}
	const TMap<FVoxelTerrainSpatialKey, FVoxelTerrainGeometryMetrics>& GetGeometryMetrics() const
	{
		return GeometryMetrics;
	}
	FVoxelTerrainWorkMetrics GetTerrainWorkMetrics() const;
	void LogTerrainGeometryMetrics() const;
	bool OnTask(FVoxelTaskResult&& InResult);
	void InvalidateSection(const FIntVector& InKey);
	void InvalidateNeighbors(const FIntVector& InKey);
	void OnContentChanged(const FIntVector& InSection);
	void OnOwnershipChanged(const FVoxelFineShardKey& InKey);
	void OnPresentationReady(const FVoxelFineShardKey& InKey);
	void OnGenerationDependencyReady(const FVoxelGenerationConsumerKey& InConsumer);

	bool ApplyRemoteRepresentation(
		const FVoxelRepresentationReply& InReply,
		FString& OutError);

	bool InvalidateRemoteRepresentations(const FVoxelRepresentationInvalidate& InInvalidate,
		int32& InOutCursor, FString& OutError, int32 InMaximumEntries = 8);

	void Reset();
	bool HasPrimaryRepresentation() const;
	FVoxelRepresentationReadiness GetRepresentationReadiness() const;
	FVoxelTerrainReadinessSnapshot CaptureTerrainReadiness() const;
	TSharedPtr<const IVoxelTerrainPresentedSnapshot, ESPMode::ThreadSafe> CapturePresentedSnapshot() const;
	bool IsFinePresented(const FIntVector& InSection) const;
	bool IsLatestFinePresented(const FIntVector& InSection) const;
	bool IsTerrainGeometryPresented(const FVoxelTerrainGeometry& InGeometry) const;
	bool RequiresSectionData(const FIntVector& InKey) const;
	FVoxelPrimaryFineReadiness GetPrimaryFineReadiness(
		const TMap<FIntVector, FVoxelExactDemand>& InExact) const;
	FVoxelPrimaryFineReadiness GetFineRadiusReadiness(
		TConstArrayView<FIntVector> InFineKeys) const;
#if WITH_DEV_AUTOMATION_TESTS
	static void SortAdmissionsByPriority(TArray<FVoxelViewAdmission>& InOutAdmissions);
	static TMap<int32, uint8> ResolveAdmissionLanes(
		TConstArrayView<FVoxelViewAdmission> InAdmissions,
		TFunctionRef<bool(const FVoxelViewAdmission&)> InIsReady);
	static int32 ResolveActiveAdmissionKind(
		TConstArrayView<FVoxelViewAdmission> InAdmissions,
		TFunctionRef<bool(const FVoxelViewAdmission&)> InIsReady,
		int32* OutPriority = nullptr);
	static double ResolveAdmissionFrontier(
		TConstArrayView<FVoxelViewAdmission> InAdmissions,
		TFunctionRef<bool(const FVoxelViewAdmission&)> InIsReady,
		double InBandWidthCells);
#endif

private:
	struct FInterestInputRoot
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
		TSet<FIntVector> FineWanted;
		TSet<FVoxelViewKey> ProxyWanted;
		TSet<FVoxelSurfaceTileKey> SurfaceWanted;
		TSet<FVoxelMacroTileKey> MacroWanted;
		FVoxelCoverageIndex TerrainRootIndex;
		TSet<FVoxelViewKey> IndexedTerrainRoots;
		uint8 MaximumTerrainRootLevel = 0;
		FVoxelFinePagePlan FinePagePlan;
	};
	struct FInterestWorkspace
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
		TSharedPtr<FInterestInputRoot, ESPMode::ThreadSafe> Root;
		TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> Snapshot;
		TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> PreviousSnapshot;
		TArray<FVoxelPublishGroupKey> ChangedCoverage;
		TArray<FVoxelPublishGroupKey> Retirements;
		TArray<FVoxelFinePageKey> LayoutChanges;
		TArray<FVoxelFineShardKey> RetiredShards;
		TArray<FVoxelFineShardKey> ChangedShardOwners;
		int32 Phase = 0;
		int32 Cursor = 0;
		bool bWaitingHierarchyCapacity = false;
		uint64 HierarchyCapacityVersion = 0;
		EVoxelTerrainResourceAdmission HierarchyAdmission = EVoxelTerrainResourceAdmission::Granted;
	};
	FInterestInputRoot EmptyInputRoot;
	TSharedPtr<FInterestInputRoot, ESPMode::ThreadSafe> AppliedInputRoot;
	TSharedPtr<FInterestInputRoot, ESPMode::ThreadSafe> CoverageInputRoot;
	bool bCoverageReadScope = false;
	TSharedPtr<FInterestInputRoot, ESPMode::ThreadSafe> RetirementInputRoot;
	bool bRetirementReadScope = false;
	FInterestInputRoot& GetInputRoot();
	const FInterestInputRoot& GetInputRoot() const;
	TSharedPtr<FInterestWorkspace, ESPMode::ThreadSafe> InterestWorkspace;
	TSharedPtr<FInterestWorkspace, ESPMode::ThreadSafe> CommittedWorkspaceEvents;
	void ReleaseInputRoot(TSharedPtr<FInterestInputRoot, ESPMode::ThreadSafe>& InRoot);
	void ReleaseInterestWorkspace(TSharedPtr<FInterestWorkspace, ESPMode::ThreadSafe>& InWorkspace);
	bool BeginInterestWorkspace(TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> InSnapshot);
	void AdvanceInterestWorkspace(double InDeadline);
	bool TryCommitInterestRoot();
	void CancelInterestWorkspace();
	void AdvanceCommittedInterestEvents(double InDeadline);
	struct FFineShardWorker;
	struct FF0FineWorker;
	friend class FVoxelFineAdmissionRetentionTest;
	struct FFineShardSlot
	{
		FVoxelFineBuildState Build;
		TSharedPtr<const void, ESPMode::ThreadSafe> RegistrationLease;
		FVoxelGenerationConsumerKey Consumer;
		FVoxelTaskStamp SubmittedStamp;
		TOptional<FVoxelFineOverlayCapture> PendingCapture;
		TSharedPtr<FFineShardWorker, ESPMode::ThreadSafe> PendingWorker;
		TSharedPtr<FF0FineWorker, ESPMode::ThreadSafe> PendingF0Worker;
		TOptional<FVoxelTaskRequest> PendingRequest;
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> AdmissionResources;
		FVoxelGenerationConsumerKey AdmissionConsumer;
		uint64 AdmissionCapacityVersion = 0;
		bool bWaitingAdmissionCapacity = false;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SamplesReadyLease;
		TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> SamplesReady;
		TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> RenderGeometry;
		// 合法 Fine Shard 最大 64 cells，最多 4^3 个 16-cell Atom；容量随 Slot 一次授予。
		TArray<TSharedPtr<FVoxelTerrainRenderAtom, ESPMode::ThreadSafe>, TInlineAllocator<64>> RenderAtoms;
		FGuid RenderTransactions[64] = {};
		EVoxelTerrainResourceAdmission RenderAdmission = EVoxelTerrainResourceAdmission::Granted;
		uint64 RenderCapacityVersion = 0;
		bool bWaitingRenderBootstrap = false;
		int32 RenderPieceCursor = 0;
		int32 RenderBatchCursor = 0;
		int32 RenderTriangleCursor = 0;
		int32 RenderLeaseCursor = -2;
		int32 RenderAtomCursor = 0;
		bool bRenderPrepared = false;
		bool bPresentationComplete = false;
		bool bCycleNeedsPresentation = false;
		double PresentationQueuedAt = 0.0;
		int32 PresentationQueueIndex = INDEX_NONE;
		uint8 PresentationQueueBand = 0;
		double FirstQueuedSeconds = 0.0;
		uint64 QueueSequence = 0;
		bool bSubmitted = false;
		bool bDependencyWake = true;
		int32 OrderIndex = INDEX_NONE;
	};
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> FineShardBackingLease;
	int32 FineShardBackingCapacity = 0;
	int32 FineShardBackingRejectedCount = 0;
	uint64 FineShardBackingReleaseVersion = 0;
	EVoxelTerrainResourceAdmission FineShardBackingAdmission = EVoxelTerrainResourceAdmission::Granted;
	TMap<FVoxelFineShardKey, FFineShardSlot> FineShardSlots;
	int64 FineShardOwnershipAtomCapacity = 0;
	TSparseArray<FVoxelFineShardKey> FineShardSlotOrder;
	int32 NativeFineRetirementCursor = 0;
	TSparseArray<FVoxelFineShardKey> FinePresentationQueues[static_cast<uint8>(EVoxelTerrainServiceBand::Count)];
	int32 FinePresentationQueueCursors[static_cast<uint8>(EVoxelTerrainServiceBand::Count)] = {};
	FGuid FineShardOwnerId = FGuid::NewGuid();
	uint64 NextFineShardIncarnation = 0;
	uint64 NextFineShardSequence = 0;
	int32 FineShardAdmissionCursor = 0;
	int32 FineShardPresentationCursor = 0;
	FVoxelTerrainWorkSelection FineShardAdmissionSelection;
	FVoxelTerrainWorkSelection FineShardPresentationSelection;
	double FineShardAdmissionCostMicroseconds = 50.0;
	double FineShardPresentationCostMicroseconds = 25.0;
	FVoxelTerrainWorkSelection TransitionAdmissionSelection[2];
	int32 TransitionAdmissionCursor[2] = {};
	double TransitionAdmissionCostMicroseconds[2] = {50.0, 50.0};
	void ProcessFineShardAdmissions();
	void PrepareFineShardPresentations();
	void EnqueueFinePresentation(const FVoxelFineShardKey& InKey);
	void OnFineOwnershipChanged(const FIntVector& InSection);
	void OnPresentationReady(const FIntVector& InSection);
	void AdvanceNativeFineRetirement();
	void RefreshTerrainStatistics();
	FVoxelTerrainMetricEvidence SampleTerrainMetric(const FVoxelTerrainMetricEvidence::FTarget& InTarget) const;
	int32 StatisticsInputPhase = 0;
	int32 StatisticsInputCursor = 0;
	int32 StatisticsEvidenceCursor = 0;
	uint64 StatisticsInputRevision = 0;
	bool IsFineGeometryPrepared(const FIntVector& InSection) const;
	bool RequestFineShard(const FVoxelFineShardKey& InKey);
	bool RequestF0Fine(const FIntVector& InSection, uint64 InRevision);
	bool ReserveFineShardSlots(int32 InCount, FString& OutError);
	bool EnsureFineShardRegistration(const FVoxelFineShardKey& InKey);
	static void ClearFineShardAdmission(FFineShardSlot& InSlot);
	static bool IsFineShardAdmissionWaiting(FFineShardSlot& InSlot);
	static void WaitFineShardAdmission(FFineShardSlot& InSlot,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, uint64 InVersion,
		const FVoxelGenerationConsumerKey* InCapacityOwner = nullptr);
	bool RejectFineShardAdmission(const FVoxelFineShardKey& InKey, const FVoxelFineBuildAttempt& InAttempt,
		const FString& InError, uint64 InCapacityVersion);
	bool SubmitFineShardAdmission(const FVoxelFineShardKey& InKey, const FVoxelFineBuildAttempt& InAttempt);
	void CancelFineShardAdmission(const FVoxelFineShardKey& InKey);
	void ApplyF0FineShardResult(const FVoxelFineShardKey& InKey, uint64 InRevision,
		const FVoxelSectionStamp& InSectionStamp, const FVoxelFineBuildAttempt& InAttempt,
		const FVoxelGenerationConsumerKey& InConsumer, FVoxelTaskResult&& InResult);
	void ApplyFineShardResult(const FVoxelFineShardKey& InKey, const FVoxelFineBuildAttempt& InAttempt,
		const FVoxelGenerationConsumerKey& InConsumer, FVoxelTaskResult&& InResult);
	void RetireFineShard(const FVoxelFineShardKey& InKey);
	struct FCoveragePassSnapshot
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
		bool bComplete = false;
		TSet<FIntVector> PresentedFineSections;
		TMap<FIntVector, FVoxelFinePageKey> PresentedFineOwners;
		TSet<FVoxelViewKey> FineNodes;
		TSet<FVoxelViewKey> ProxyNodes;
		TSet<FVoxelViewKey> FineBranches;
		TSet<FVoxelViewKey> ProxyBranches;
		TArray<FVoxelCoverageBox> Fine;
		TArray<FVoxelCoverageBox> Proxy;
		TArray<FVoxelCoverageRect> ProxySurface;
		TArray<FVoxelCoverageRect> Surface;
		TArray<FVoxelCoverageRect> Macro;
		TArray<FVoxelCoverageRect> Heightfield;
		TArray<FVoxelCoverageRect> WantedHeightfield;
	};
	FVoxelCoverageIndex CoverageIndex;
	FVoxelCoverageIndex HeightfieldCoverageIndex;
	TMap<FVoxelTerrainSpatialKey, uint64> CoverageIndexRetries;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CoverageIndexRetryLease;
	int32 CoverageIndexRetryCapacity = 0;
	int32 CoverageIndexRetryCursor = 0;
	void BindCoverageIndexResources();
	bool ReserveCoverageIndexRetries(int32 InCount);
	bool ApplyCoverageIndexRecord(const FVoxelCoverageRecord& InRecord);
	void AdvanceCoverageIndexRecovery(double InDeadline);
	void IndexCoverage(const FVoxelViewKey& InKey);
	void IndexCoverage(const FVoxelSurfaceTileKey& InKey);
	void IndexCoverage(const FVoxelMacroTileKey& InKey);
	void RefreshIndexedCoverage(const FVoxelCoverageRect& InBounds);
	void CaptureCoverageCandidates(TSet<FIntVector>& OutFine, TSet<FVoxelViewKey>& OutProxy, TSet<FVoxelSurfaceTileKey>& OutSurface, TSet<FVoxelMacroTileKey>& OutMacro) const;
	mutable uint64 CoverageIndexNodesVisited = 0;
	mutable uint64 CoverageRecordsExamined = 0;
	mutable uint64 CoverageRecordsCaptured = 0;
	mutable uint64 CoverageReadGuards = 0;
	void BuildCoveragePassSnapshot(FCoveragePassSnapshot& OutSnapshot) const;
	bool IsTerrainVolumeCovered(const FVoxelViewKey& InNode, const FCoveragePassSnapshot& InSnapshot, bool bByFine) const;
	FVoxelCoverageRect TerrainCoverageRect(const FVoxelViewKey& InKey) const;
	void GatherFinePageCandidates(const TSet<FVoxelViewKey>& InCandidates, TSet<FVoxelFinePageKey>& OutPages) const;
	bool IsFineReplacementReady(const FIntVector& InKey, const FCoveragePassSnapshot& InSnapshot) const;
	bool IsProxyReplacementReady(const FVoxelViewKey& InKey, const FCoveragePassSnapshot& InSnapshot) const;
	bool IsSurfaceReplacementReady(const FVoxelSurfaceTileKey& InKey, const FCoveragePassSnapshot& InSnapshot) const;
	bool IsMacroReplacementReady(const FVoxelMacroTileKey& InKey, const FCoveragePassSnapshot& InSnapshot) const;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FVoxelFineShardAttemptIsolationTest;
	friend class FVoxelCriticalDependencyAdmissionTest;
	friend class FVoxelFinePageObsoleteDirtyCleanupTest;
	friend class FVoxelManagerLocalCoverageTest;
	friend class FVoxelInterestInputBudgetTest;
	friend class FVoxelEmptyTransitionConvergenceTest;
	friend class FVoxelFineProxyDependenciesTest;
	friend class FVoxelFinePageCoverageTest;
	friend class FVoxelKnownEmptyPageIntegrationTest;
	friend class FVoxelDeferredRepresentationAdmissionTest;
#endif
	void UpdateWantedTimestamps(double InNow);
	bool IsCriticalDependency(const FVoxelViewAdmission& InAdmission) const;
	bool IsCriticalDependency(const FVoxelTerrainSpatialKey& InKey) const;
	void ProcessAdmissions();
	void ProcessDataAdmissions();
	void ProcessServiceAdmissions(bool bInDataOnly);
	bool ReserveServiceAdmissionAges(int32 InStage, int32 InCount);
	void PruneServiceAdmissionAges(int32 InStage, double InDeadline);
	bool IsServiceAdmissionIdentityCurrent(const FVoxelTaskKey& InIdentity) const;
	void RecordServiceResourceAdmission(EVoxelTerrainResourceAdmission InAdmission);
	void EnumerateServiceResourceSources(FVoxelGenerationRequiredSourceScope::FVisit InVisit) const;
	bool IsAdmissionDataReady(const FVoxelViewAdmission& InAdmission, bool bIncludePending = false) const;
	bool IsAdmissionMeshReady(const FVoxelViewAdmission& InAdmission) const;
	bool IsAdmissionPresented(const FVoxelViewAdmission& InAdmission) const;
	void RefreshTerrainBuildStage();
	bool IsFineCoverageReady(const FIntVector& InSection, const IVoxelTerrainPresentedSnapshot* InPresented) const;
	void RebuildFinePagePlan();
	void MarkFinePageDirty(const FVoxelFinePageKey& InPage);
	void InvalidateFinePageLayout(const FVoxelFinePageKey& InPage);
	void RetireFinePageLayoutState(const FVoxelFinePageKey& InPage);
	FString LastFinePagePlanningError;
	void InvalidateFinePresentation(const FIntVector& InSection);
	void InvalidateRepresentationKeys(const FVoxelAffectedRepresentationKeys& InAffected);
	bool IsPreparedDataCurrent(const FVoxelTaskKey& InKey) const;
	FVoxelTaskStamp MakeRepresentationStamp(const FVoxelTerrainGeometry& InGeometry, uint64 InRevision) const;
	bool IsRepresentationStampCurrent(const FVoxelTaskStamp& InStamp) const;
	bool HasDeferredRepresentation(const FVoxelTaskStamp& InStamp) const;
	void PrunePreparedData();
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> RetainPreparedEnvironment(
		const FVoxelTaskKey& InKey, const FVoxelGenerationPipeline& InGenerator,
		const FVoxelGenerationBounds& InBounds);
	bool EnqueuePreparedData(FVoxelTaskRequest&& InRequest);
	void RemovePreparedData(const FVoxelTaskKey& InKey);
	bool TryAcceptPreparedRevision(const FVoxelTaskKey& InKey, FVoxelTaskResult& InOutResult);
	TSharedPtr<const FVoxelTaskResult, ESPMode::ThreadSafe> FindPreparedData(const FVoxelTaskKey& InKey);
	void RebuildAdmissions(TConstArrayView<FVector> InObservers);
	void UpdateTaskPriorities();
	bool ResolveTaskServicePriority(EVoxelTaskKind InKind, const FVoxelTaskStamp& InStamp,
		EVoxelWorkClass& InOutWorkClass, FVoxelTerrainServicePriority& OutPriority) const;
	int32 VoxelProxyTerrainStage(const FVoxelViewKey& InKey) const;
	int32 AdmissionTerrainStage(const FVoxelViewAdmission& InAdmission) const;
	EVoxelWorkClass VolumeTransitionWorkClass(const FVoxelViewKey& InOwner) const;
	void TrackReadyTerrainNode(FVoxelViewKey InKey);
	void RebuildReadyTerrainBranches();
	bool CommitTerrainOwnership(const FVoxelPublishGroupKey& InKey, TConstArrayView<FVoxelViewKey> InTarget);
	double MinimumObserverDistanceCells(const FVector& InWorldCenter) const;
	double MinimumObserverDistanceCells(const FVoxelGenerationBounds& InCellBounds) const;
	bool IsAdmissionTerminalFailure(const FVoxelViewAdmission& InAdmission) const;
	bool TrySubmitAdmission(const FVoxelViewAdmission& InAdmission);
	void CancelStaleViewTasks();
	FVector SurfaceWorldCenter(const FVoxelSurfaceTileKey& InKey) const;
	FVector MacroWorldCenter(const FVoxelMacroTileKey& InKey) const;

	FVoxelGameThreadContinuation IsTerrainVolumeCoveredSteps(
		FVoxelViewKey InNode, const FCoveragePassSnapshot& InSnapshot, bool bByFine, bool& OutCovered) const;

	bool EnqueueViewTask(FVoxelTaskRequest&& InRequest);
	void RecordTerrainMesh(const FVoxelTerrainSpatialKey& InKey, const FVoxelSectionMeshResult& InMesh, double InBuildMilliseconds);
	void LogRepresentationState(TConstArrayView<FVector> InObservers);
	void MarkCoverageDirty();
	void MarkCoverageDirty(const FVoxelCoverageRect& InBounds);
	static void MergeCoverageRect(TArray<FVoxelCoverageRect>& InOutRects, const FVoxelCoverageRect& InBounds);
	void ExpandActiveCoverage(const FVoxelCoverageRect& InBounds);
	void RetryActiveCoverage();
	FGuid BeginPublishGroup(const FVoxelPublishGroupKey& InKey);
	bool CoverageAffects(const FVoxelCoverageRect& InBounds) const;
	bool ShouldDeferCoverage(const FVoxelCoverageRect& InBounds) const;
	bool ShouldDeferCoverage(const FVoxelTaskStamp& InStamp) const;
	FVoxelCoverageRect CoverageBounds(const FVoxelTaskStamp& InStamp) const;
	void InvalidateRepresentationKey(const FVoxelTerrainSpatialKey& InKey);
	void ResolveTransitionVisibility();
	FVoxelGameThreadContinuation ResolveTransitionVisibilitySteps();
	FVoxelGameThreadContinuation CaptureCoverageCandidateSteps(
		TSet<FIntVector>& OutFine, TSet<FVoxelViewKey>& OutProxy, TSet<FVoxelSurfaceTileKey>& OutSurface, TSet<FVoxelMacroTileKey>& OutMacro,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease, bool& OutComplete) const;
	FVoxelGameThreadContinuation AcquireCoverageCapacitySteps(uint64 InBytes,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InOutLease, bool& OutComplete) const;
	FVoxelGameThreadContinuation BuildCoverageSnapshotSteps(FCoveragePassSnapshot& OutSnapshot) const;
	FVoxelGameThreadContinuation RebuildHeightfieldTransitionSteps(const FCoveragePassSnapshot& InSnapshot);
	FVoxelGameThreadContinuation RebuildVolumeTransitionSteps(const TSet<FVoxelViewKey>& InTargetNodes);
	void RebuildHeightfieldTransitions(const FCoveragePassSnapshot& InSnapshot);
	void ApplyHeightfieldTransition(
		const FVoxelHeightfieldNodeKey& InOwner,
		uint64 InSignature,
		FVoxelTaskResult&& InResult);
	void PumpHeightfieldTransitions();
	void RebuildVolumeTransitions(const TSet<FVoxelViewKey>& InTargetNodes);
	bool VolumeHandoffAffectsHeightfield(const FVoxelHeightfieldNodeKey& InKey) const;
	void PumpVolumeTransitions();
	TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> FindFineBoundaryGeometry(const FIntVector& InSection) const;
	bool AreBoundaryInputsCurrent(const FVoxelBoundaryTransitionContext& InContext) const;
	bool AreHeightfieldInputsCurrent(const FVoxelFrozenHeightfieldInputs& InInputs) const;
	void ApplyVolumeTransition(const FVoxelViewKey& InOwner, uint64 InSignature,
		FVoxelTaskResult&& InResult);
	void CleanupRetiredRepresentations(double InNow);
	FVoxelGameThreadContinuation CleanupRetiredRepresentationSteps(double InNow, FVoxelPublishGroupKey InCandidate);
	bool ReleaseRetirementGeometry(const FVoxelTerrainSpatialKey& InKey);
	bool IsRetirementWanted(const FVoxelPublishGroupKey& InCandidate) const;
	void ResetRetirementContinuation();
	void QueueRetirement(const FVoxelPublishGroupKey& InKey);
	void RemoveRetirement(const FVoxelPublishGroupKey& InKey);

	FVoxelCoverageBox FineCoverageBox(const FIntVector& InKey) const;
	FVoxelCoverageBox VoxelProxyCoverageBox(const FVoxelViewKey& InKey) const;
	FVoxelCoverageRect FineCoverageRect(const FIntVector& InKey) const;
	FVoxelCoverageRect VoxelProxyCoverageRect(const FVoxelViewKey& InKey) const;
	FVoxelCoverageRect SurfaceCoverageRect(const FVoxelSurfaceTileKey& InKey) const;
	FVoxelCoverageRect MacroCoverageRect(const FVoxelMacroTileKey& InKey) const;
	void GatherReadyWantedFineBoxes(TArray<FVoxelCoverageBox>& OutCoverage) const;
	void GatherReadyWantedProxyBoxes(TArray<FVoxelCoverageBox>& OutCoverage) const;
	void GatherReadyWantedSurfaceRects(TArray<FVoxelCoverageRect>& OutCoverage) const;
	void GatherReadyWantedMacroRects(TArray<FVoxelCoverageRect>& OutCoverage) const;
	void GatherReadyWantedProxySurfaceRects(const FVoxelCoverageRect& InTarget, TArray<FVoxelCoverageRect>& OutCoverage) const;
	void UpdateProxySurfaceCoverage(TSharedPtr<const FVoxelVoxelProxyData, ESPMode::ThreadSafe> InData);
	void GatherCurrentRenderDomainRects(TArray<FVoxelCoverageRect>& OutCoverage) const;
	bool IsFineReplacementReady(const FIntVector& InKey) const;
	bool IsProxyReplacementReady(const FVoxelViewKey& InKey) const;
	bool IsSurfaceReplacementReady(const FVoxelSurfaceTileKey& InKey) const;
	bool IsMacroReplacementReady(const FVoxelMacroTileKey& InKey) const;
	bool IsFineInsideRenderDomain(const FIntVector& InKey) const;
	bool IsProxyInsideRenderDomain(const FVoxelViewKey& InKey) const;
	bool IsSurfaceInsideRenderDomain(const FVoxelSurfaceTileKey& InKey) const;
	bool IsMacroInsideRenderDomain(const FVoxelMacroTileKey& InKey) const;
	static bool HasRenderableMesh(const FVoxelSectionMeshResult& InMesh);
	bool TryResolveFineWithoutMesh(const FIntVector& InSection, uint64 InRevision);
	static bool BuildVoxelProxySnapshot(
		const FVoxelVoxelProxyData& InData,
		uint64 InRecipeHash,
		FVoxelSectionSnapshot& OutSnapshot,
		FString& OutError);

	bool RequestFine(const FIntVector& InSection, uint64 InRevision);
	bool RequestVoxelProxy(const FVoxelViewKey& InKey, int32 InPriority, bool bDataOnly = false);
	bool RequestSurface(const FVoxelSurfaceTileKey& InKey, int32 InPriority, bool bDataOnly = false);
	bool RequestMacro(const FVoxelMacroTileKey& InKey, int32 InPriority, bool bDataOnly = false);

	bool PublishFine(const FVoxelTaskResult& InResult);
	bool PublishVoxelProxy(const FVoxelTaskResult& InResult);
	bool PublishSurface(const FVoxelTaskResult& InResult);
	void PublishWater(const FVoxelTaskResult& InResult);
	bool PublishMacro(const FVoxelTaskResult& InResult);

private:
	static constexpr double RetireDelaySeconds = 0.20;
	static constexpr double OutsideDomainRetireDelaySeconds = 0.50;

	UVoxelModule& Module;
	FVoxelTaskScheduler& Scheduler;
	uint64 WorldEpoch = 0;
	FGuid TaskOwnerId = FGuid::NewGuid();
	uint64 TaskOwnerIncarnation = 1;
	mutable uint64 NextRepresentationAttempt = 0;
	struct FRepresentationAttempt
	{
		FVoxelTaskStamp Stamp;
		// Scheduler 交出结果后，覆盖回调消费前仍属于同一请求。
		EVoxelTaskKind DeferredKind = EVoxelTaskKind::None;
	};
	mutable TMap<FVoxelTerrainSpatialKey, FRepresentationAttempt> RepresentationAttempts;
	uint64 AppliedInterestRevision = 0;
	bool bTerrainBuildStageDirty = true;
	bool bTaskPrioritiesDirty = false;
	TConstArrayView<FVoxelViewAdmission> Admissions;
	FVoxelPublishBudget& TerrainFrameBudget;
	bool bTerrainFrameBudgetActive = false;
	int32 NextManagerPhase = 0;
	int32 NextAdmissionPhase = 0;
	TSharedPtr<const FVoxelTerrainPublicationInputs, ESPMode::ThreadSafe> PendingPublicationInputs;
	TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> PendingInterestSnapshot;
	TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> QueuedInterestSnapshot;
	TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> AppliedInterestSnapshot;
	bool bConsumingPublicationInputs = false;
	const FVoxelInterestSet& GetViewInterest() const;
	uint64 PendingInterestRevision = 0;
	int32 InterestInputPhase = 0;
	int32 InterestInputCursor = 0;
	bool bPendingPageLayoutChanged = false;
	FVoxelFinePagePlan PendingFinePagePlan;
	TSet<FVoxelFinePageKey> PendingLayoutInvalidations;
	double TerrainDeadline(double InMaximumMilliseconds) const;
	TArray<FVector> PriorityObservers;
	TMap<int32, int32> AdmissionScanIndices[4];
	TMap<int32, int32> DataScanIndices[4];
	TMap<int32, int32> DataFrontierScanIndices[4];
	struct FAdmissionServiceAge
	{
		double FirstQueuedSeconds = 0.0;
		uint64 Sequence = 0;
	};
	FVoxelTerrainWorkSelection AdmissionSelection[2];
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> AdmissionAgeLeases[2];
	TMap<FVoxelTaskKey, FAdmissionServiceAge> AdmissionAges[2];
	int32 AdmissionAgeCapacities[2] = {};
	int32 AdmissionAgeCleanupCursors[2] = {};
	bool bAdmissionAgeCapacityWaiting[2] = {};
	uint64 AdmissionAgeCapacityVersions[2] = {};
	bool bServiceSelectionCapacityWaiting[2] = {};
	uint64 ServiceSelectionCapacityVersions[2] = {};
	EVoxelTerrainResourceAdmission LastServiceResourceAdmission = EVoxelTerrainResourceAdmission::None;
	int32 ServiceAdmissionScans[2][4] = {};
	double AdmissionCostEstimates[2][4] = {};
	uint64 NextAdmissionSequence = 0;
	TConstArrayView<int32> AdmissionPriorities;
	TMap<FVoxelTaskKey, TSharedPtr<FVoxelTaskResult, ESPMode::ThreadSafe>> PreparedData;
	TMap<FVoxelTaskKey, TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe>> PreparedEnvironmentRetentions;
	uint64 PreparedDataBytes = 0;
	uint64 PendingDataBytes = 0;
	uint64 SkippedProxyMeshes = 0;
	uint64 SkippedVolumeMeshes = 0;
	double AdmissionBandWidthCells = 64.0;
	double LastResolvedFrontier = 0.0;
	int32 LastActiveAdmissionKind = 4;
	int32 LastActiveAdmissionPriority = MAX_int32;
	int32 LastActiveDataKind = 4;
	int32 NextDataPriority = 0;
	int32 NextMeshPriority = 0;
	double ManagerPhaseEstimateMilliseconds[static_cast<uint8>(EVoxelPublishPhase::Count)] = {};
	double InterestMicrostepEstimateMilliseconds = 0.01;
	uint64 ManagerPhaseDeferrals[static_cast<uint8>(EVoxelPublishPhase::Count)] = {};
	double NextInterestProgressLog = 0.0;
	double NextPlanTimingLog = 0.0;
	double NextPointHandoffLog = 0.0;
	double NextTimingLog = 0.0;
	FVoxelGameThreadContinuation CoverageContinuation;
	TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> CoverageInterestSnapshot;
	FVoxelGameThreadContinuation RetirementContinuation;
	TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> RetirementInterestSnapshot;
	FVoxelCoverageIndex RetirementCoverageRoot;
	FVoxelCoverageIndex RetirementHeightfieldRoot;
	FVoxelChangeHierarchySnapshot RetirementContentSnapshot;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> RetirementHierarchyResources;
	FVoxelGenerationConsumerKey RetirementHierarchyConsumer;
	EVoxelTerrainResourceAdmission RetirementHierarchyAdmission = EVoxelTerrainResourceAdmission::Granted;
	uint64 RetirementHierarchyCapacityVersion = 0;
	FIntVector RetirementContentMin;
	FIntVector RetirementContentMax;
	FVoxelCoverageRect RetirementBounds;
	FVoxelPublishGroupKey RetirementCandidate;
	TArray<FVoxelCoverageRect> RetirementScopeRects;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetirementScopeLease;
	TArray<FVoxelTaskResult> DeferredCoverageResults;
	int32 DeferredCoverageResultCursor = 0;
	struct FDeferredCoverageCallback
	{
		FVoxelCoverageRect Bounds;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
		TFunction<void()> Apply;

		FDeferredCoverageCallback() = default;
		FDeferredCoverageCallback(const FVoxelCoverageRect& InBounds, TFunction<void()> InApply,
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InResourceLease = nullptr)
			: Bounds(InBounds), ResourceLease(MoveTemp(InResourceLease)), Apply(MoveTemp(InApply))
		{
		}
		FDeferredCoverageCallback(FDeferredCoverageCallback&& InOther)
			: Bounds(InOther.Bounds), ResourceLease(MoveTemp(InOther.ResourceLease)), Apply(MoveTemp(InOther.Apply))
		{
		}
		FDeferredCoverageCallback& operator=(FDeferredCoverageCallback&& InOther)
		{
			if (this != &InOther)
			{
				Apply = nullptr;
				ResourceLease = MoveTemp(InOther.ResourceLease);
				Bounds = InOther.Bounds;
				Apply = MoveTemp(InOther.Apply);
			}
			return *this;
		}
		FDeferredCoverageCallback(const FDeferredCoverageCallback&) = delete;
		FDeferredCoverageCallback& operator=(const FDeferredCoverageCallback&) = delete;
	};
	TQueue<FDeferredCoverageCallback> DeferredCoverageCallbacks;
	double NextBudgetLog = 0.0;
	mutable double NextIncompleteFineLog = 0.0;
	double CoverageStartedSeconds = 0.0;
	uint64 CoverageMicrosteps = 0;
	uint64 CoverageContentRevision = 0;
	bool bCoverageRetryRequested = false;
	bool bCoverageDirty = true;
	bool bCoverageDirtyFull = true;
	bool bActiveCoverageFull = true;
	TArray<FVoxelCoverageRect> DirtyCoverageRects;
	TArray<FVoxelCoverageRect> ActiveDirtyCoverageRects;
	template<typename TPlan>
	struct TLocalCoverageState
	{
		uint64 DesiredSignature = 0;
		uint64 PendingSignature = 0;
		TSharedPtr<const TPlan, ESPMode::ThreadSafe> Plan;
	};
	uint64 HeightfieldPlanSerial = 0;
	TMap<FVoxelHeightfieldNodeKey, TLocalCoverageState<FVoxelHeightfieldCoveragePlan>> HeightfieldCoveragePlans;
	TSet<FVoxelHeightfieldNodeKey> PendingHeightfieldPlanOwners;
	uint64 VolumePlanSerial = 0;
	TMap<FVoxelViewKey, TLocalCoverageState<FVoxelVolumeCoveragePlan>> VolumeCoveragePlans;
	FVoxelCoverageIndex VolumePlanIndex;
	TArray<FVoxelCoverageBox> PendingVolumeHandoffBoxes;
	TArray<FVoxelPublishGroupKey> RetirementQueue;
	TMap<FVoxelPublishGroupKey, int32> RetirementIndices;
	int32 RetirementCursor = 0;
	double NextRetireCheck = 0.0;
	double LastCoverageMilliseconds = 0.0;
	double LastRetireMilliseconds = 0.0;
	TMap<FVoxelTerrainSpatialKey, FVoxelTerrainGeometryMetrics> GeometryMetrics;
	TMap<FVoxelTerrainSpatialKey, FVoxelTerrainGeometryMetrics> PresentedGeometryMetrics;
	FVoxelTerrainWorkMetrics TerrainWorkMetrics;
	FVoxelFrameTimings FrameTimings;
	double NextRepresentationDebugLog = 0.0;
	TUniquePtr<FVoxelViewPublisher> Publisher;
	TMap<FVoxelPublishGroupKey, FGuid> OpenPublishGroups;
	TMap<FVoxelPublishGroupKey, FGuid> ActiveCoveragePublishGroups;

	TSet<FVoxelViewKey> VisibleTerrainNodes;
	TMap<FVoxelPublishGroupKey, TArray<FVoxelViewKey>> VisibleTerrainNodesByGroup;
	TSet<FVoxelViewKey> ReadyTerrainBranches;
	bool bReadyTerrainBranchesDirty = true;
	TSet<FIntVector> FineReady;
	TSet<FVoxelViewKey> VoxelProxyReady;
	TSet<FVoxelSurfaceTileKey> SurfaceReady;
	TSet<FVoxelMacroTileKey> MacroReady;

	TMap<FIntVector, TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>> FineSectionMeshes;
	TMap<FIntVector, FVoxelFinePageKey> PresentedFineSectionOwners;
	TSet<FVoxelFinePageKey> DirtyFinePages;
	TSet<FVoxelFinePageKey> PendingFinePages;
	bool bFinePresentationDirty = true;
	bool bHeightfieldTopologyDirty = true;
	uint64 FinePageLayoutRevision = 0;
	mutable uint64 CoverageSnapshotBuildCount = 0;
	uint64 HeightfieldTopologyRebuildCount = 0;
	uint64 TerrainCandidateCount = 0;
	uint64 FullTerrainActorScanCount = 0;
	TMap<FIntVector, uint64> FineRevisions;

	TMap<FVoxelViewKey, TObjectPtr<AActor>> VoxelProxyActors;
	TMap<FVoxelViewKey, uint64> VoxelProxyRevisions;

	TMap<FVoxelSurfaceTileKey, TObjectPtr<AActor>> SurfaceActors;
	TMap<FVoxelSurfaceTileKey, TObjectPtr<AActor>> WaterActors;
	TMap<FVoxelMacroTileKey, TObjectPtr<AActor>> MacroActors;
	TMap<FVoxelHeightfieldNodeKey, TObjectPtr<AActor>> HeightfieldTransitionActors;
	TMap<FVoxelHeightfieldNodeKey, uint64> HeightfieldTransitionSignatures;
	TMap<FVoxelHeightfieldNodeKey, uint64> DesiredTransitionSignatures;
	TMap<FVoxelHeightfieldNodeKey, TSharedPtr<const FVoxelFrozenHeightfieldInputs, ESPMode::ThreadSafe>> DesiredHeightfieldInputs;
	uint64 NextHeightfieldInputSerial = 0;
	TMap<FVoxelHeightfieldNodeKey, uint64> PendingTransitionSignatures;
	TMap<FVoxelHeightfieldNodeKey, uint64> PreparedTransitionSignatures;
	TMap<FVoxelHeightfieldNodeKey, TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe>> PreparedTransitionMeshes;
	TMap<FVoxelHeightfieldNodeKey, TArray<FVoxelHeightfieldTransitionEdge>> DesiredTransitionEdges;
	TArray<FVoxelHeightfieldNodeKey> UnsubmittedTransitionOwners;
	TArray<FVoxelCoverageRect> PendingHeightfieldHandoffRects;
	int32 HeightfieldUnbalancedEdgeCount = 0;
	TMap<FVoxelViewKey, uint64> VolumeTransitionSignatures;
	uint64 NextVolumeInputSerial = 0;
	TMap<FVoxelViewKey, uint64> DesiredVolumeSignatures;
	TMap<FVoxelViewKey, uint64> PendingVolumeSignatures;
	TMap<FVoxelViewKey, uint64> PreparedVolumeSignatures;
	TMap<FVoxelViewKey, TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe>> PreparedVolumeMeshes;
	TMap<FVoxelViewKey, TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe>> PreparedVolumeBoundaryMeshes;
	TMap<FVoxelViewKey, TObjectPtr<AActor>> VolumeBoundaryActors;
	TMap<FVoxelViewKey, TSharedPtr<const FVoxelBoundaryTransitionContext>> DesiredVolumeContexts;
	TArray<FVoxelViewKey> UnsubmittedVolumeOwners;
	int32 VolumeUnbalancedFaceCount = 0;
	int32 MissingFineBoundaryCount = 0;
	int32 MissingProxyBoundaryCount = 0;
	int32 InvalidVolumeContextCount = 0;

	TMap<FVoxelSurfaceTileKey, uint64> SurfaceRevisions;
	TMap<FVoxelMacroTileKey, uint64> MacroRevisions;

	TMap<FVoxelViewKey, TSharedPtr<const FVoxelVoxelProxyData, ESPMode::ThreadSafe>> VoxelProxyData;
	TSet<FVoxelViewKey> AirVoxelProxyNodes;
	TMap<FVoxelViewKey, TSharedPtr<const FVoxelVoxelProxyData, ESPMode::ThreadSafe>> ProxySurfaceCoverage;
	TMap<FVoxelSurfaceTileKey, TSharedPtr<const FVoxelSurfaceTileData, ESPMode::ThreadSafe>> SurfaceData;
	TMap<FVoxelSurfaceTileKey, TSharedPtr<const FVoxelWaterSurfaceTileData, ESPMode::ThreadSafe>> WaterData;
	TMap<FVoxelMacroTileKey, TSharedPtr<const FVoxelMacroTileData, ESPMode::ThreadSafe>> MacroData;

	TMap<FIntVector, double> FineLastWanted;
	TMap<FVoxelViewKey, double> VoxelProxyLastWanted;
	TMap<FVoxelSurfaceTileKey, double> SurfaceLastWanted;
	TMap<FVoxelMacroTileKey, double> MacroLastWanted;
};
