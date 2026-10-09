#pragma once

#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelOverlaySnapshot.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Tasks/Task.h"

struct FVoxelCommittedJournalNode;
struct FVoxelJournalSaveNotifications;

// 捕获只复制根引用；已提交事实和落盘证明使用同一固定切点。
struct WHFRAMEWORK_API FVoxelCommittedJournalCapture
{
	// 最后销毁此引用：根字段先退引用，避免 Worker deleter 抢先释放保护根。
	TSharedPtr<FVoxelJournalSaveNotifications, ESPMode::ThreadSafe> SaveNotifications;
	TSharedPtr<const FVoxelCommittedJournalNode, ESPMode::ThreadSafe> Root;
	TSharedPtr<const FVoxelCommittedJournalNode, ESPMode::ThreadSafe> PersistedRoot;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	uint64 WorldEpoch = 0;
	uint64 CommitCut = 0;
	FGuid OwnerId;
	uint64 PersistedCut = 0;

	bool PrepareSaveNotifications(const FGuid& InTransactionId,
		EVoxelTerrainResourceAdmission* OutAdmission = nullptr);
	bool VisitDirty(TFunctionRef<bool(const FVoxelOverlaySnapshot&)> InVisit) const;
	TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> FindDirty(const FIntVector& InSection) const;
};

class WHFRAMEWORK_API FVoxelCommittedJournal
{
public:
	explicit FVoxelCommittedJournal(uint64 InWorldEpoch);
	~FVoxelCommittedJournal();
	void BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources);
	bool PreparePublication(const FIntVector& InSection, uint64 InRevision,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease) const;
	void Publish(TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> InJournal);
	TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> Find(const FIntVector& InSection) const;
	uint64 PersistedRevision(const FIntVector& InSection) const;
	uint64 CommittedRevision(const FIntVector& InSection) const;
	FVoxelCommittedJournalCapture Capture(uint64 InCommitCut) const;
	bool AcceptSave(const FVoxelCommittedJournalCapture& InCapture);
	bool CanAcceptSave(const FVoxelCommittedJournalCapture& InCapture) const;
	bool AdvanceSaveNotifications(FIntVector& OutSection, uint64& OutRevision);
	bool HasSaveNotifications() const;
	bool MarkPersisted(const FIntVector& InSection, uint64 InRevision);
	void Visit(const FVoxelGenerationBounds& InBounds, TFunctionRef<void(const FIntVector&)> InVisit) const;
	void AdvanceRetirement(int32 InMaximumNodes, double InDeadline,
		TFunctionRef<bool(const FIntVector&)> InIsResident);
	bool IsRetirementIdle() const;
	uint64 GetPersistedCut() const;

private:
	using FNodePtr = TSharedPtr<const FVoxelCommittedJournalNode, ESPMode::ThreadSafe>;
	bool EnsureReclaimLeases() const;
	FNodePtr Root;
	FNodePtr PersistedRoot;
	TSharedPtr<FVoxelJournalSaveNotifications, ESPMode::ThreadSafe> SaveNotificationsHead;
	FVoxelJournalSaveNotifications* SaveNotificationsTail = nullptr;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FGuid OwnerId = FGuid::NewGuid();
	FGuid ReclaimOwnerId = FGuid::NewGuid();
	uint64 WorldEpoch = 0;
	uint64 SavedCut = 0;
	FNodePtr RetirementPath[33];
	uint8 RetirementChild[33] = {};
	int32 RetirementDepth = INDEX_NONE;
	UE::Tasks::FTask ReclaimTask;
	mutable TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ReclaimTaskLease;
	mutable TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ShutdownTaskLease;
};
