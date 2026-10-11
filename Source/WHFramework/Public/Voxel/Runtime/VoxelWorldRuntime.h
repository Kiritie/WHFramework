#pragma once

#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionSnapshot.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Runtime/VoxelChangeHierarchy.h"
#include "Voxel/Runtime/VoxelChangeIndex.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Runtime/VoxelSection.h"
#include "Voxel/Runtime/VoxelOverlaySnapshot.h"
#include "Voxel/Runtime/VoxelCommittedJournal.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Save/VoxelDeltaTypes.h"

class FVoxelTerrainFrameContext;

enum class EVoxelRuntimeSectionEvent : uint8
{
	Allocated,
	Wanted,
	IdentityChanging,
	Removing,
	PinReleased,
	Committed,
	Persisted
};

struct WHFRAMEWORK_API FVoxelPreparedSection
{
	FVoxelPreparedSection() = default;
	FVoxelPreparedSection(const FVoxelPreparedSection&) = delete;
	FVoxelPreparedSection& operator=(const FVoxelPreparedSection&) = delete;
	FVoxelPreparedSection(FVoxelPreparedSection&&) = default;
	FVoxelPreparedSection& operator=(FVoxelPreparedSection&&) = default;

	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> DataLease;
	FVoxelSectionStamp Before;
	TArray<FVoxelBlockState> Blocks;
	TMap<int32, FVoxelBlockState> Overlay;
	TMap<int32, FVoxelBlockEntityState> Entities;
	FVoxelSectionPatch Patch;
};

struct WHFRAMEWORK_API FVoxelPreparedEdit
{
	FVoxelPreparedEdit() = default;
	~FVoxelPreparedEdit();
	FVoxelPreparedEdit(const FVoxelPreparedEdit&) = delete;
	FVoxelPreparedEdit& operator=(const FVoxelPreparedEdit&) = delete;
	FVoxelPreparedEdit(FVoxelPreparedEdit&&) = default;
	FVoxelPreparedEdit& operator=(FVoxelPreparedEdit&& InOther);

	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FGuid TransactionId;
	TArray<FVoxelPreparedSection> Sections;
};


struct WHFRAMEWORK_API FVoxelInventorySectionTarget
{
    uint64 ExpectedRevision = 0;
    FVoxelPersistentSection After;
};

struct WHFRAMEWORK_API FVoxelPreparedInventorySection
{
    FIntVector Section = FIntVector::ZeroValue;
    FVoxelPreparedSection Data;
    TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> Journal;
    FVoxelRuntimeSectionIdentity Resource;
    bool bLoaded = false;
    bool bPinned = false;
};

// 所有正文、Journal、失效目录及网络 Patch 存储在 durable point 前授额。
struct WHFRAMEWORK_API FVoxelPreparedInventoryCommit
{
    bool bPrepared = false;
    uint64 WorldEpoch = 0;
    uint64 HierarchyRevision = 0;
    TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ScratchLease;
    TArray<FVoxelPreparedInventorySection> Sections;
    FVoxelPreparedChangeInvalidations Invalidations;
    FVoxelEditBatch Batch;
};

class WHFRAMEWORK_API FVoxelWorldRuntime
{
public:
	FVoxelWorldRuntime(
		uint64 InWorldEpoch,
		bool bInAuthority,
		TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
		TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources);
#if WITH_DEV_AUTOMATION_TESTS
	FVoxelWorldRuntime(uint64 InWorldEpoch, bool bInAuthority,
		TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
		TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator);
#endif
	~FVoxelWorldRuntime();

	FVoxelWorldRuntime(const FVoxelWorldRuntime&) = delete;
	FVoxelWorldRuntime& operator=(const FVoxelWorldRuntime&) = delete;

	FVoxelSection* FindSection(const FIntVector& InSection);
	const FVoxelSection* FindSection(const FIntVector& InSection) const;
	FVoxelSection* FindOrAllocate(const FIntVector& InSection, uint64 InFrame);
	TArray<FIntVector> ResidentSections() const;
	// 稳定稀疏次序供有界领域工作区使用；退休由事件队列推进。
	const TSparseArray<FIntVector>& GetResidencyOrder() const;
	bool TryPinSection(const FIntVector& InSection, const FVoxelRuntimeSectionIdentity& InIdentity);
	bool ReleaseSectionPin(const FIntVector& InSection, const FVoxelRuntimeSectionIdentity& InIdentity);
	void SetSectionEventCallback(TFunction<void(const FIntVector&, const FVoxelSectionStamp&, EVoxelRuntimeSectionEvent)> InCallback);

	bool TryGetBlock(const FIntVector& InPosition, FVoxelBlockState& OutState) const;
	bool PublishBase(
		const FIntVector& InSection,
		const FVoxelSectionStamp& InStamp,
		TArray<FVoxelBlockState>&& InBaseBlocks,
		FString& OutError,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InResourceLease = nullptr);
	bool PublishFinal(
		const FIntVector& InSection,
		uint64 InRevision,
		const TMap<int32, FVoxelBlockState>& InOverlay,
		const TMap<int32, FVoxelBlockEntityState>& InEntities,
		FString& OutError);
	bool CaptureSnapshot(const FIntVector& InSection, FVoxelSectionSnapshot& OutSnapshot,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InResourceLease = nullptr) const;
	// 原已授额快照只补齐一个缺失面，不重分配或重捕已冻结正文及邻面。
	bool CompleteSnapshotHalo(FVoxelSectionSnapshot& InOutSnapshot, uint8 InFace) const;

	bool PrepareEdit(
		const TArray<FVoxelCellEdit>& InCells,
		const TArray<FVoxelEntityEdit>& InEntities,
		FVoxelPreparedEdit& OutPrepared,
		FString& OutError) const;
	bool CommitPreparedEdit(
		FVoxelPreparedEdit&& InPrepared,
		FVoxelEditBatch& OutBatch,
		FString& OutError);

    bool PrepareInventorySections(TConstArrayView<FVoxelInventorySectionTarget> InTargets,
        const FGuid& InTransactionId, bool bInRecovery, FVoxelPreparedInventoryCommit& OutPrepared, FString& OutError);
    bool ValidateInventorySections(const FVoxelPreparedInventoryCommit& InPrepared, FString& OutError) const;
    bool CommitInventorySections(FVoxelPreparedInventoryCommit& InPrepared, FVoxelEditBatch& OutBatch, FString& OutError);
    void CancelInventorySections(FVoxelPreparedInventoryCommit& InPrepared);
    bool RestoreInventorySection(const FVoxelPersistentSection& InSection, FString& OutError);
    void PublishInventorySections(const FVoxelEditBatch& InBatch);
	bool ApplyPatch(const FVoxelSectionPatch& InPatch, FString& OutError);
	bool ApplyRemotePatchBatch(const FVoxelEditBatch& InBatch, FString& OutError);

	bool RemoveSection(const FIntVector& InSection, bool bDiscardModified = false);
	void MarkCommitted(const FIntVector& InSection, uint64 InRevision);
	uint64 GetPersistedRevision(const FIntVector& InSection) const;
	uint64 GetCommittedOverlayRevision(const FIntVector& InSection) const;
	FVoxelCommittedJournalCapture CaptureJournal() const;
	bool CompleteJournalSave(const FVoxelCommittedJournalCapture& InCapture);
	bool CanCompleteJournalSave(const FVoxelCommittedJournalCapture& InCapture) const;
	bool HasPendingJournalSaveNotifications() const;
	void AdvanceJournalRetirement(int32 InMaximumNodes, double InDeadline, FVoxelTerrainFrameContext* InFrameContext = nullptr);
	bool IsJournalRetirementIdle() const;
	bool HasUnpersistedChanges() const;
	bool IsCurrentStamp(const FIntVector& InSection, const FVoxelSectionStamp& InStamp) const;

	int32 NumSections() const;
	uint64 Epoch() const;
	bool IsServer() const;

	FVoxelChangeHierarchy& GetChangeHierarchy();
	const FVoxelChangeHierarchy& GetChangeHierarchy() const;
	FVoxelChangeIndex& GetChangeIndex();
	const FVoxelChangeIndex& GetChangeIndex() const;
	void BindTerrainResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources);
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> GetTerrainResources() const;
	void AdvanceResidentPromotion(int32 InMaximumSections, double InDeadline, FVoxelTerrainFrameContext* InFrameContext = nullptr);
	TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> FindCommittedOverlay(const FIntVector& InSection) const;
	uint64 GetAuthorityCommitCut() const;
	uint64 GetOverlayRegionSequence(const FIntVector& InRegion) const;
	void GatherCommittedOverlaySections(const FVoxelGenerationBounds& InBounds, TArray<FIntVector>& OutSections) const;
	void VisitCommittedOverlaySections(const FVoxelGenerationBounds& InBounds,
		TFunctionRef<void(const FIntVector&)> InVisit) const;
	uint64 GetContentRevision() const;
	uint64 GetDataReadyRevision() const { return DataReadyRevision; }
	bool InvalidateRemoteSection(const FIntVector& InSection, FString& OutError);
#if WITH_DEV_AUTOMATION_TESTS
	void InvalidateRemoteSection(const FIntVector& InSection)
	{
		FString Error;
		check(InvalidateRemoteSection(InSection, Error));
	}
#endif
	void SetRemoteSectionValidator(TFunction<bool(const FIntVector&, uint64)> InValidator);

private:
	friend class FVoxelResidencyManager;
	void NotifySectionEvent(const FIntVector& InSection, EVoxelRuntimeSectionEvent InEvent);
	bool ValidateState(const FVoxelBlockState& InState) const;
	bool ValidateSection(
		const TArray<FVoxelBlockState>& InBlocks,
		const TMap<int32, FVoxelBlockState>& InOverlay,
		const TMap<int32, FVoxelBlockEntityState>& InEntities,
		FString& OutError) const;
	bool BuildPatchCandidate(
		const FVoxelSection& InSection,
		const FVoxelSectionPatch& InPatch,
		TArray<FVoxelBlockState>& OutBlocks,
		TMap<int32, FVoxelBlockState>& OutOverlay,
		TMap<int32, FVoxelBlockEntityState>& OutEntities,
		FString& OutError,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutDataLease) const;
	void PublishPatchCandidate(
		FVoxelSection& InSection,
		const FVoxelSectionPatch& InPatch,
		TArray<FVoxelBlockState>&& InBlocks,
		TMap<int32, FVoxelBlockState>&& InOverlay,
		TMap<int32, FVoxelBlockEntityState>&& InEntities,
		TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> InJournal,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InDataLease);
	bool ReserveRuntimeStorage(int32 InSectionCount);
	bool AcquireRuntimeData(const FIntVector& InSection, uint64 InBytes, bool bInReplacing,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease) const;
	void FinishRuntimeData(FVoxelSection& InSection, const FIntVector& InKey,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease);
	void RetireRuntimeData(FVoxelSection& InSection);
	bool PrepareJournal(const FIntVector& InSection, uint64 InRevision,
		const TMap<int32, FVoxelBlockState>& InOverlay,
		const TMap<int32, FVoxelBlockEntityState>& InEntities,
		TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>& OutJournal, FString& OutError,
		bool bInPersisted = false) const;
	void RetainJournal(const FIntVector& InSection,
		TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> InJournal, bool bInNotify = true);
	void AdvanceOverlaySequences(TConstArrayView<FVoxelSectionPatch> InPatches);
	bool ReserveOverlaySequences(TConstArrayView<FVoxelSectionPatch> InPatches);
	bool ReserveOverlaySequenceKeys(TConstArrayView<FIntVector> InSections);

private:
	uint64 WorldEpoch = 0;
	uint64 DataReadyRevision = 0;
	uint64 NextToken = 1;
	bool bAuthority = false;
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> StorageLease;
	int32 StorageCapacity = 0;
	TMap<FIntVector, TSharedPtr<FVoxelSection, ESPMode::ThreadSafe>> Sections;
	TSparseArray<FIntVector> ResidencyOrder;
	// 物理容量随驻留目录扩容预授予；逻辑操作仅由唯一 ResidencyManager 持有。
	TArray<FVoxelSection*> RetirementTimers;
	TSet<FIntVector> PendingResidentPromotion;
	int32 ResidentPromotionCursor = 0;
	uint64 ResidentPromotionReleaseVersion = MAX_uint64;
	int32 ResidentPromotionRemaining = 0;
	double ResidentPromotionCostMilliseconds = 0.01;
	FVoxelChangeHierarchy ChangeHierarchy;
	FVoxelChangeIndex ChangeIndex;
	FVoxelCommittedJournal CommittedJournal;
	double JournalNotificationCostMilliseconds = 0.01;
	double JournalRetirementCostMilliseconds = 0.01;
	bool bNextJournalNotification = true;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> OverlaySequenceLease;
	int32 OverlaySequenceCapacity = 0;
	struct FOverlayRegionSequence
	{
		uint64 Value = 0;
		uint64 LastCommitCut = 0;
	};
	TMap<FIntVector, FOverlayRegionSequence> OverlayRegionSequences;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> TerrainResources;
	FGuid RuntimeOwnerId = FGuid::NewGuid();
	mutable uint64 NextAllocationSerial = 0;
	FGuid JournalOwnerId = FGuid::NewGuid();
	uint64 AuthorityCommitCut = 0;
	TFunction<bool(const FIntVector&, uint64)> RemoteSectionValidator;
	TFunction<void(const FIntVector&, const FVoxelSectionStamp&, EVoxelRuntimeSectionEvent)> SectionEventCallback;
};

