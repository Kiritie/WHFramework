#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelViewSourceId.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Rendering/VoxelPublishGroups.h"
#include "Voxel/Rendering/VoxelPublishBudget.h"
#include "Voxel/Rendering/VoxelGameThreadContinuation.h"
#include "Voxel/Task/VoxelTerrainWorkSelection.h"
#include "Voxel/Rendering/VoxelTerrainRenderer.h"

class UVoxelModule;
class UVoxelMeshComponent;
class FVoxelTerrainRenderer;
struct FVoxelTerrainCoarseMesh;
struct FVoxelTerrainRenderCoarse;
struct FVoxelFineOwnerEvidence;
class FVoxelTerrainRenderChunk;

struct WHFRAMEWORK_API FVoxelViewCoverageResult final : FVoxelTaskCustomPayload
{
	TArray<TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>> Meshes;
	uint64 OwnedMeshBytes = 0;
	virtual uint64 GetAllocatedBytes() const override;
};

class WHFRAMEWORK_API FVoxelViewPublisher
{
public:
	FVoxelViewPublisher(UVoxelModule& InModule, FVoxelTaskScheduler& InScheduler, uint64 InWorldEpoch);
	~FVoxelViewPublisher();
	FGuid BeginGroup(const FVoxelPublishGroupKey& InKey);
	bool Stage(
		const FGuid& InGroup,
		FVoxelViewSourceId& InOutSourceId,
		const FVector& InLocation,
		double InScale,
		FVoxelSectionMeshResult&& InMesh,
		int32 InTerrainStage,
		bool bInTransition = false);
	bool StageImmutable(const FGuid& InGroup, FVoxelViewSourceId& InOutSourceId,
		const FVector& InLocation, double InScale,
		TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe> InMesh,
		int32 InTerrainStage, bool bInTransition = false);
	bool SetCoverage(const FGuid& InGroup, FVoxelViewSourceId InSourceId, TArray<FBox> InWorldCellBoxes,
		TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes = nullptr);
	bool SetSharedCoverage(const FGuid& InGroup, FVoxelViewSourceId InSourceId,
		TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes);
	FVoxelGameThreadContinuation SetCoverageSteps(const FGuid& InGroup, FVoxelViewSourceId InSourceId, TArray<FBox> InWorldCellBoxes,
		TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes, bool& OutSuccess);
	FVoxelGameThreadContinuation SetSharedCoverageSteps(
		const FGuid& InGroup, FVoxelViewSourceId InSourceId, TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes, bool& OutSuccess);
	bool SetScopedCoverage(const FGuid& InGroup, FVoxelViewSourceId InSourceId, TArray<FBox> InWorldCellBoxes,
		TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes,
		TConstArrayView<FBox> InAffectedWorldCellBoxes, bool bInRetainPresented = false);
	FVoxelGameThreadContinuation SetScopedCoverageSteps(const FGuid& InGroup, FVoxelViewSourceId InSourceId, TArray<FBox> InWorldCellBoxes,
		TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes, TConstArrayView<FBox> InAffectedWorldCellBoxes, bool& OutSuccess,
		bool bInRetainPresented = false);
	bool SetHidden(const FGuid& InGroup, FVoxelViewSourceId InSourceId, bool bInHidden);
	void SetCommitCallbacks(TFunction<void()> InOnCommitted,
		TFunction<void(TConstArrayView<FVoxelPublishGroupKey>)> InOnGroupCommitted, TConstArrayView<FGuid> InGroups = {});
	// 状态版本由门禁拥有者保管，须比Publisher存活更久；状态变化后仍通知提交就绪失效。
	void SetCommitGate(TFunction<bool(const FVoxelPublishGroupKey&)> InGate, const uint64& InStateRevision)
	{
		check(CommitGateBindingRevision < MAX_uint64);
		++CommitGateBindingRevision;
		CommitGateStateRevision = &InStateRevision;
		CommitGate = MoveTemp(InGate);
		InvalidateCommitReadiness();
	}
	void InvalidateCommitReadiness();
	bool CommitGroup(const FGuid& InGroup);
	bool DiscardUnchangedGroup(const FGuid& InGroup);
	void SetValidation(const FGuid& InGroup, TFunction<bool()> InValidation);
	TArray<FVoxelPublishGroupKey> TakeRetryOwners();
	bool SetOwnershipChanged(const FGuid& InGroup, bool bInChanged);
	bool SetTerrainStage(const FGuid& InGroup, int32 InTerrainStage);
	void SetObservers(TConstArrayView<FVector> InObservers);
	void Tick();
	void Tick(FVoxelPublishBudget& InBudget, bool bInAllowCommit = true, double InDeadlineSeconds = 0.0);
	void InvalidatePriorities();
	void Forget(FVoxelViewSourceId InSourceId);
	bool Retire(FVoxelViewSourceId InSourceId);
	void Reset();
	bool IsBusy() const;
	bool HasActiveGroups() const { return !Publications.IsEmpty(); }
	bool HasReadyCommits() const
	{
		return !ReadyCommitGroups.IsEmpty();
	}
	bool IsGroupBusy(const FVoxelPublishGroupKey& InKey) const;
	bool NeedsUpdate() const;
	bool IsHidden(FVoxelViewSourceId InSourceId) const;
	bool IsPresented(FVoxelViewSourceId InSourceId) const;
	bool IsCommitted(FVoxelViewSourceId InSourceId) const;
	bool HasPresentation(FVoxelViewSourceId InSourceId) const;
	bool GetPresentedMeshCost(FVoxelViewSourceId InSourceId, uint64& OutVertices, uint64& OutTriangles, uint64& OutBytes) const;
	FVoxelFrameTimings GetFrameTimings() const { return FrameTimings; }
	FVoxelTerrainRenderer& GetTerrainRenderer() const { return *TerrainRenderer; }
	bool IsCurrentAtomEvidence(const FIntVector& InSection, const FVoxelFineOwnerEvidence& InEvidence) const;
	void SetGeometryValidator(TFunction<bool(const FVoxelTaskStamp&)> InValidator)
	{
		GeometryValidator = MoveTemp(InValidator);
	}
	void SetMeshValidator(TFunction<bool(const FVoxelSectionMeshResult&)> InValidator)
	{
		MeshValidator = MoveTemp(InValidator);
	}

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FVoxelScopedCoverageContinuationTest;
	friend class FVoxelPublishEstimateRecoveryTest;
	friend class FVoxelPublisherProgressAndNearOrderTest;
	friend class FVoxelConcurrentPublishCommitTest;
	friend class FVoxelFrozenGenerationProgressTest;
	friend class FVoxelPublisherStageAndIndexTest;
	friend class FVoxelFinePageCoverageTest;
	friend class FVoxelKnownEmptyPageIntegrationTest;
#endif
	using FMeshPtr = TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>;
	using FGroupCallback = TFunction<void(TConstArrayView<FVoxelPublishGroupKey>)>;
	using FGroupCallbackPtr = TSharedPtr<FGroupCallback>;
	struct FGroupPriority
	{
		FVoxelTerrainServicePriority Service;
		// GT 距离按世界 XY 平方；worker 距离按 streaming source 的 cell 空间。
		double ObserverDistance = MAX_dbl;
		double DistanceScore = MAX_dbl;
		int32 SourcePriority = MAX_int32;
		int32 TerrainStage = 3;
		EVoxelWorkClass WorkClass = EVoxelWorkClass::Visible;
	};
	struct FDeferredPriorityDemand
	{
		TArray<FBox> Bounds;
		uint64 Serial = 0;
		int32 TerrainStage = 3;
		bool bTransition = false;
	};
	using FPriorityBoundsPtr = TSharedPtr<const FDeferredPriorityDemand, ESPMode::ThreadSafe>;

	struct FEntry
	{
		FVoxelTaskStamp NativeStamp;
		FVoxelTaskStamp PresentedNativeStamp;
		TSharedPtr<const FVoxelTerrainCoarseMesh, ESPMode::ThreadSafe> NativeMesh;
		FMeshPtr Source;
		FMeshPtr PresentedSource;
		uint64 PresentedVertices = 0;
		uint64 PresentedTriangles = 0;
		uint64 PresentedBytes = 0;
		int32 TerrainStage = INDEX_NONE;
		FVector Location = FVector::ZeroVector;
		double Scale = 1.0;
		FBox Bounds = FBox(ForceInit);
		FBox PresentedWorldBounds = FBox(ForceInit);
		FVoxelPublishGroupKey GroupKey;
		TArray<FBox> Exclusions;
		TArray<FBox> DesiredWorldExclusions;
		bool bHasDesiredExclusions = false;
		TArray<FBox> LocalWorldBoxes;
		TArray<TObjectPtr<UVoxelMeshComponent>> Components;
		bool bTransition = false;
		bool bDirty = true;
		bool bPresented = false;
		bool bHidden = false;
		bool bRetiring = false;
	};

	struct FUpdate
	{
		FVoxelTaskStamp NativeStamp;
		TSharedPtr<const FVoxelTerrainCoarseMesh, ESPMode::ThreadSafe> NativeMesh;
		bool bNativeMeshPending = false;
		bool bNativeAdmissionWaiting = false;
		uint64 NativeAdmissionVersion = 0;
		FVoxelViewSourceId SourceId;
		FMeshPtr Source;
		int32 TerrainStage = INDEX_NONE;
		TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> CommonBoxes;
		FBox Bounds = FBox(ForceInit);
		TArray<FBox> PreviousExclusions;
		TArray<FBox> ChangedBounds;
		TArray<FBox> LocalWorldBoxes;
		bool bSourceDirty = true;
		bool bTransition = false;
		bool bRetainSource = false;
		bool bUnchanged = false;
		int32 GroupIndex = INDEX_NONE;
		int32 PreparedBatchIndex = 0;
		FVector Location = FVector::ZeroVector;
		double Scale = 1.0;
		TArray<FBox> Exclusions;
		TArray<TObjectPtr<UVoxelMeshComponent>> Components;
	};

	struct FGroup
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> NativeGroupLease;
		TArray<FVoxelTerrainCoarseMutation> NativeChanges;
		// 粗层发布共享原始索引范围，局部裁切随 Fine 在同一呈现根提交。
		TSharedPtr<FVoxelTerrainRenderCoarse, ESPMode::ThreadSafe> NativeCoarse;
		TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe> NativeLastChunk;
		FGuid NativeTransaction;
		int32 NativeSourceCursor = 0;
		int32 NativeBatchCursor = 0;
		int32 NativeTriangleCursor = 0;
		bool bNativeSubmitted = false;
		// 从索引准备到呈现回执共用准入槽，避免等待提交的正文耗尽常驻池。
		bool bNativeAdmitted = false;
		FVoxelGenerationConsumerKey NativeControlConsumer;
		uint64 NativeControlVersion = 0;
		uint64 NativeFrontendVersion = 0;
		bool bNativeControlWaiting = false;
		bool bNativeBootstrapWaiting = false;
		FBox Bounds = FBox(ForceInit);
		TArray<FBox> ChangedBounds;
		// Proxy 接缝可能服务于 Fine 交接，实际更新阶段不能仅由 owner 类别推断。
		int32 TerrainStage = 3;
		bool bTransition = false;
		TArray<int32> UpdateIndices;
		TArray<FVoxelViewSourceId> VisibilitySources;
		TArray<FVoxelPublishGroupKey> Keys;
		FGroupPriority Priority;
		double FirstQueuedAt = 0.0;
		uint64 QueueSequence = 0;
		int32 PreparedUpdates = 0;
		bool bCommitted = false;
		bool bPrepareQueued = false;
		bool bCommitQueued = false;
		bool bPriorityReady = false;
		uint64 PrepareWaitTick = 0;
		uint64 CommitWaitTick = 0;
		mutable uint64 CommitGateBlockedRevision = 0;
		mutable uint64 CommitGateBlockedBindingRevision = 0;
		mutable uint64 CommitGateBlockedGroupRevision = 0;
		mutable uint64 DependencyNextSerial = 0;
		mutable uint64 DependencyQuerySerial = 0;
		mutable uint64 DependencyQueryRevision = 0;
		mutable bool bDependencyQueryActive = false;
		mutable FVoxelPublishBoundsIndex::FQuery DependencyQuery;
	};

	struct FPendingGroup
	{
		FVoxelPublishGroupKey Key;
		bool bCommitted = false;
		FGroupCallbackPtr Callback;
		bool bOwnershipChanged = true;
		FPriorityBoundsPtr DeferredPriorityBounds;
		int32 TerrainStage = 3;
		TSharedPtr<TFunction<bool()>> Validation;
	};

	struct FPreparedPlan
	{
		TArray<FVoxelPublishFootprint> Footprints;
		TMap<FVoxelViewSourceId, int32> SourceIndices;
		TMap<FVoxelViewSourceId, const FUpdate*> Updates;
		TSet<FVoxelPublishGroupKey> SourceKeys;
		TSet<FVoxelPublishGroupKey> GroupKeys;
		TArray<FGroup> Groups;
		FVoxelPublishGroupContinuation Graph;
		int32 Phase = 0;
		int32 Cursor = 0;
		int32 GraphCursor = 0;
	};
	struct FPublication
	{
		uint64 Serial = 0;
		uint64 GroupRevision = 1;
		uint64 IndexedGroupRevision = 0;
		int32 DependencyIndexBuildCursor = 0;
		uint64 DependencyIndexBuildRevision = 0;
		FVoxelPublishBoundsIndex DependencyIndex;
		TUniquePtr<FPreparedPlan> Plan;
		TArray<FUpdate> Updates;
		TArray<FGroup> Groups;
		TArray<int32> GroupOrder;
		TMap<FVoxelViewSourceId, bool> Visibility;
		TMap<FVoxelPublishGroupKey, FGroupCallbackPtr> Callbacks;
		TMap<FVoxelPublishGroupKey, TSharedPtr<TFunction<bool()>>> Validations;
		TMap<FVoxelPublishGroupKey, int32> TerrainStages;
		TSet<FVoxelPublishGroupKey> OwnershipChanges;
		TMap<FVoxelPublishGroupKey, FPriorityBoundsPtr> DeferredPriorityBounds;
		TArray<FMeshPtr> PreparedMeshes;
		FVoxelGameThreadContinuation CoverageWork;
		int32 CoverageIndex = 0;
		TArray<int32> BuildOrder;
		TSet<int32> PendingBuildIndices;
		int32 BuildInitializationIndex = 0;
		int32 BuildHeapifyIndex = INDEX_NONE;
		bool bBuildOrderInitialized = false;
		bool bBuildOrderDirty = true;
		bool bCoveragePrepared = false;
		bool bCoveragePreparing = false;
		bool bGroupsPrepared = true;
		bool bFinished = false;
		bool bCanceled = false;
	};
	using FPublicationRef = TSharedRef<FPublication>;
	struct FPendingPlan
	{
		TMap<FGuid, FPendingGroup> Groups;
		TMap<FVoxelPublishGroupKey, FGuid> GroupIds;
		TArray<FUpdate> Updates;
		TMap<FVoxelViewSourceId, int32> UpdateIndices;
		TMap<FVoxelViewSourceId, bool> Visibility;
		TArray<FGuid> Handles;
		TArray<FVoxelPublishFootprint> Footprints;
		TMap<FVoxelPublishGroupKey, int32> KeyIndices;
		TSet<int32> BlockedComponents;
		TMap<int32, int32> SelectedComponents;
		TMap<FVoxelPublishGroupKey, int32> SelectedKeys;
		TMap<FVoxelViewSourceId, FVoxelPublishGroupKey> SourceKeys;
		FVoxelPublishGroupContinuation Graph;
		FPublicationRef Publication = MakeShared<FPublication>();
		int32 Phase = 0;
		int32 Cursor = 0;
		bool bCapacityFailure = false;
	};

	struct FPriorityGroup
	{
		FPublicationRef Key;
		int32 Value = INDEX_NONE;
		uint64 Revision = 0;
		bool IsCurrent() const
		{
			return !Key->bCanceled && !Key->bFinished && Key->GroupRevision == Revision && Key->Groups.IsValidIndex(Value);
		}
	};
	struct FPriorityPlan
	{
		TArray<FPublicationRef> Publications;
		TArray<FPriorityGroup> Groups;
		TArray<FPriorityBoundsPtr> Deferred;
		FVoxelPublishBoundsIndex SpatialIndex;
		FVoxelPublishBoundsIndex::FQuery SpatialQuery;
		FGroup SpatialDemand;
		bool bSpatialQueryActive = false;
		TSet<const FDeferredPriorityDemand*> SeenDeferred;
		TMap<FVoxelPublishGroupKey, FGroupPriority> Priorities;
		uint64 Revision = 0;
		int32 Phase = 0;
		int32 Cursor = 0;
		int32 InnerCursor = 0;
		int32 PairCursor = 0;
	};

	struct FSourceUpdateGeneration
	{
		FPublicationRef Publication;
		int32 UpdateIndex = INDEX_NONE;
	};
	struct FSourceVisibilityGeneration
	{
		FPublicationRef Publication;
		bool bHidden = false;
	};
	struct FReadyGroup
	{
		FPublicationRef Publication;
		int32 GroupIndex = INDEX_NONE;
		FVoxelTerrainServicePriority Priority;
		uint64 PriorityRevision = MAX_uint64;
	};
	static bool HigherReadyCommit(const FReadyGroup& InFirst, const FReadyGroup& InSecond);
	void InsertReadyCommit(FReadyGroup InReady);
	void InsertReadyPrepare(FReadyGroup InReady);
	void IndexPublicationUpdate(const FPublicationRef& Publication, int32 InIndex);
	void ReleaseSourceGeneration(FVoxelViewSourceId InSourceId, uint64 InSerial);
	void QueueReadyGroup(const FPublicationRef& InPublication, int32 InGroupIndex);
	void RebuildFirstOwners();
	void SetOwnerActive(const FVoxelPublishGroupKey& InKey, uint64 InSerial, bool bInActive);
	void InvalidateDependencyProofs(FPublication& InPublication);
	int32 SelectReadyGroup(TConstArrayView<FReadyGroup> InReady, FVoxelTerrainWorkSelection& InSelection,
		FVoxelTerrainServiceCharge& OutCharge, bool bInCommit = false, double InDeadline = MAX_dbl, bool* bOutAdvanced = nullptr);
	void SettleReadyGroup(FVoxelTerrainWorkSelection& InSelection, const FVoxelTerrainServiceCharge& InCharge,
		double InStartedSeconds, bool bInCommit);
	bool HasOlderDependency(const FPublicationRef& InPublication, const FGroup& InGroup, bool* bOutNeedsContinuation = nullptr) const;
	FPendingGroup* FindPendingGroup(const FGuid& InGroup);
	void StartCommittedGroups();
	void AdvancePendingPlan(double InDeadlineSeconds = MAX_dbl);
	void AdvancePreparedPlan(const FPublicationRef& Publication, double InDeadlineSeconds = MAX_dbl);
	void BuildGroups(const FPublicationRef& Publication, bool bInPreparedCoverage = false);
	void DeferSaturatedGroups(const FPublicationRef& Publication, TMap<FVoxelPublishGroupKey, FGroupPriority>& OutPriorities);
	void AdvancePriorities();
	void UpdateGroupPriorities(TMap<FVoxelPublishGroupKey, FGroupPriority> InPriorities);
	FUpdate TakePendingUpdate(int32 InIndex);
	void SortGroups(const FPublicationRef& Publication);
	void RefreshGroupOrder(const FPublicationRef& Publication);
	double GroupDistance(const FGroup& InGroup) const;
	double BoundsDistance(TConstArrayView<FBox> InBounds) const;
	static int32 GroupStage(const FGroup& InGroup);
	bool CanPublishGroup(const FGroup& InGroup) const;
	static void MergePriority(FGroupPriority& InOutPriority, const FGroupPriority& InPriority);
	static bool HigherGroupPriority(const FGroupPriority& InFirst, const FGroupPriority& InSecond);
	FGroupPriority BoundsPriority(TConstArrayView<FBox> InBounds, int32 InStage) const;
	FGroupPriority GroupPriority(const FGroup& InGroup) const;
	static bool GroupsInteract(const FGroup& InFirst, const FGroup& InSecond);
	void SetPublicationPriority(FVoxelTaskRequest& InOutRequest, const FPublication& InPublication, TConstArrayView<int32> InGroupIndices) const;
	void RefreshTaskPriorities();
	bool AdmitCoveragePreparation(const FPublicationRef& Publication, double InDeadline = MAX_dbl);
	FVoxelGameThreadContinuation PrepareCoverageSteps(FPublication& InPublication);
	static bool IsProjectedCoveragePrepared(const FPublication* InPublication, const FUpdate* InUpdate);
	const FUpdate* FindProjectedUpdate(FVoxelViewSourceId InSourceId, uint64 InBeforeSerial = MAX_uint64,
		const FPublication** OutPublication = nullptr) const;
	bool CaptureCoverageSource(FVoxelViewSourceId InSourceId, bool bInRetainPresented, FUpdate& OutSource) const;
	FVoxelGameThreadContinuation SetCoverageSourceSteps(const FGuid& InGroup, FVoxelViewSourceId InSourceId, FUpdate InSource,
		TArray<FBox> InWorldCellBoxes, TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes, bool& OutSuccess);
	bool HigherBuildPriority(const FPublication& Publication, int32 First, int32 Second) const;
	bool AdmitBuilds(const FPublicationRef& Publication, double InDeadline = MAX_dbl);
	enum class ENativeUpdateWork : uint8
	{
		None,
		Upload,
		Remove
	};
	ENativeUpdateWork GetNativeUpdateWork(const FPublication& InPublication, const FUpdate& InUpdate) const;
	bool HasNativeGroupWork(const FPublication& InPublication, const FGroup& InGroup) const;
	bool PrepareUpdate(const FPublicationRef& Publication, int32 InIndex, double InDeadline, int32& InOutPreparedComponents, bool& bOutComplete);
	bool CommitReadyGroups(const FPublicationRef& Publication, int32& InOutCommitted, double InDeadline,
		const TMap<FVoxelPublishGroupKey, uint64>& InFirstOwners, int32 InGroupIndex = INDEX_NONE);
	bool CommitReadyGroup(const FPublicationRef& Publication, FGroup& InGroup);
	void DiscardReadyGroup(const FPublicationRef& InPublication, FGroup& InGroup);
	bool AdvanceNativeGroup(const FPublicationRef& InPublication, FGroup& InGroup, bool& bOutAdvanced);
	void ReleaseNativeGroup(FGroup& InGroup);
	bool IsNativeGeometryCurrent(const FVoxelTaskStamp& InStamp) const;
	bool RunBudgetedPhase(FVoxelPublishBudget& InBudget, EVoxelPublishPhase InPhase, TFunctionRef<bool()> InOperation,
		double InDeadlineSeconds = MAX_dbl);
	double PhaseOperationEstimateMilliseconds[static_cast<uint8>(EVoxelPublishPhase::Count)] = {};
	uint64 PhaseConsecutiveDeferrals[static_cast<uint8>(EVoxelPublishPhase::Count)] = {};
	double NextPublisherDiagnostic = 0.0;
	void FinishGroups();
	void DiscardGroups();
	void DiscardPublication(const FPublicationRef& Publication, bool bInAllowRetry = true);

private:
	UVoxelModule& Module;
	FVoxelTaskScheduler& Scheduler;
	uint64 WorldEpoch = 0;
	mutable TOptional<FVoxelTerrainRenderer> TerrainRenderer;
	uint64 BatchSerial = 0;
	FGuid NativeOwnerId = FGuid::NewGuid();
	static constexpr int32 MaximumNativeGroups = 8;
	int32 ActiveNativeGroups = 0;
	uint64 NativeSourceSerial = 0;
	TMap<FVoxelViewSourceId, FEntry> Entries;
	TArray<FVoxelPublishGroupKey> RetryOwners;
	TMap<FVoxelPublishGroupKey, TSet<FVoxelViewSourceId>> OwnerSources;
	TMap<FGuid, FPendingGroup> PendingGroups;
	TMap<FVoxelPublishGroupKey, FGuid> PendingGroupIds;
	TArray<FUpdate> PendingUpdates;
	TMap<FVoxelViewSourceId, int32> PendingUpdateIndices;
	TMap<FVoxelViewSourceId, bool> PendingVisibility;
	TUniquePtr<FPendingPlan> PendingPlan;
	TArray<FPublicationRef> Publications;
	TMap<FVoxelViewSourceId, TArray<FSourceUpdateGeneration>> SourceUpdateGenerations;
	TMap<FVoxelViewSourceId, TArray<FSourceVisibilityGeneration>> SourceVisibilityGenerations;
	// 发布分组的派生查询索引，避免每个 SourceId 重扫全部在途 owner。
	mutable TSet<FVoxelPublishGroupKey> ActiveBusyKeys;
	mutable bool bActiveBusyKeysDirty = true;
	TArray<FVector> Observers;
	TArray<TWeakObjectPtr<UVoxelMeshComponent>> RetiredComponents;
	TFunction<void()> OnCommitted;
	TFunction<bool(const FVoxelPublishGroupKey&)> CommitGate;
	const uint64* CommitGateStateRevision = nullptr;
	uint64 CommitGateBindingRevision = 0;
	TFunction<bool(const FVoxelTaskStamp&)> GeometryValidator;
	TFunction<bool(const FVoxelSectionMeshResult&)> MeshValidator;
	FGroupCallbackPtr CurrentGroupCallback;
	TSharedPtr<bool> CommitCallbackLifetime;
	TArray<FReadyGroup> ReadyPrepareGroups;
	TArray<FReadyGroup> ReadyCommitGroups;
	FVoxelTerrainWorkSelection PrepareSelection;
	FVoxelTerrainWorkSelection CommitSelection;
	double ServiceCostEstimates[2][static_cast<uint8>(EVoxelTerrainServiceBand::Count)] = {};
	int32 ReadySelectionCursors[2] = {};
	uint64 PublisherTickSerial = 0;
	uint64 CommitGateCacheHits = 0;
	double NextReadyCandidateDiagnostic[2] = {};
	uint64 PrepareReadinessRevision = 0;
	uint64 CommitReadinessRevision = 0;
	uint64 CommitResourceVersion = 0;
	uint64 CommitFrontendVersion = 0;
	bool bCommitOrderDirty = false;
	bool bCommitSelectionComplete = false;
	bool bCommitNeedsPolling = false;
	uint64 ReadySequence = 0;
	TMap<FVoxelPublishGroupKey, uint64> OwnerFirstSerial;
	TMap<FVoxelPublishGroupKey, TArray<uint64>> OwnerSerialQueues;
	bool bFirstOwnersDirty = true;
	uint64 PriorityRevision = 0;
	TUniquePtr<FPriorityPlan> PriorityPlan;
	bool bPrioritiesDirty = true;
	int32 NextPhase = 0;
	int32 AdmissionCursor = 0;
	bool bRetry = false;
	FVoxelFrameTimings FrameTimings;
};
