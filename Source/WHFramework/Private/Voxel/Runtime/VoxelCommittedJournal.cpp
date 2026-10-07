#include "Voxel/Runtime/VoxelCommittedJournal.h"

#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

struct FVoxelCommittedJournalNode
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PathLease;
	TSharedPtr<const FVoxelCommittedJournalNode, ESPMode::ThreadSafe> Children[8];
	TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> Journal;
	uint64 Revision = 0;
	uint64 BasePersistedRevision = 0;
};

// 路径只借用本次固定根的节点；队列不复制叶或增加一份权威版本表。
struct FVoxelJournalSaveNotifications
{
	TSharedPtr<const FVoxelCommittedJournalNode, ESPMode::ThreadSafe> Root;
	TSharedPtr<const FVoxelCommittedJournalNode, ESPMode::ThreadSafe> PersistedRoot;
	TSharedPtr<FVoxelJournalSaveNotifications, ESPMode::ThreadSafe> Next;
	const FVoxelCommittedJournalNode* Path[33] = {};
	const FVoxelCommittedJournalNode* PersistedPath[33] = {};
	uint8 Child[33] = {};
	int32 Depth = 0;
	bool bQueued = false;

	~FVoxelJournalSaveNotifications()
	{
		// 世界停止时迭代拆解整条旧队列；最后引用和旧树仍由 Worker deleter 释放。
		while (Next && Next.IsUnique())
		{
			auto Node = MoveTemp(Next);
			Next = MoveTemp(Node->Next);
		}
	}
};

namespace
{
	using FNodePtr = TSharedPtr<const FVoxelCommittedJournalNode, ESPMode::ThreadSafe>;
	uint64 PathBytes()
	{
		return 33 * VoxelBoundedAllocation::SharedBytes<FVoxelCommittedJournalNode>();
	}
	struct FJournalRootReclaimer
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> TaskLease;
		FNodePtr Roots[35];
		void operator()() const
		{
			// 唯一引用的旧树只在Worker析构；不捕获World或Runtime。
		}
	};

	uint32 Encode(const int32 InValue)
	{
		return static_cast<uint32>(InValue) ^ 0x80000000u;
	}

	int32 ChildAt(const FIntVector& InSection, const int32 InDepth)
	{
		const int32 Shift = 31 - InDepth;
		return ((Encode(InSection.X) >> Shift) & 1u) |
			(((Encode(InSection.Y) >> Shift) & 1u) << 1) |
			(((Encode(InSection.Z) >> Shift) & 1u) << 2);
	}

	const FVoxelCommittedJournalNode* FindNode(const FNodePtr& InRoot, const FIntVector& InSection)
	{
		const FVoxelCommittedJournalNode* Node = InRoot.Get();
		for (int32 Depth = 0; Depth < 32 && Node; ++Depth)
		{
			Node = Node->Children[ChildAt(InSection, Depth)].Get();
		}
		return Node;
	}

	FNodePtr SetNode(const FNodePtr& InRoot, const FIntVector& InSection, const int32 InDepth,
		const TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>& InJournal,
		const uint64 InRevision, const uint64 InPersisted,
		const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InLease)
	{
		// 整条固定 33 节点路径在调用之前授予，旧子树只共享引用。
		auto Node = InRoot ? MakeShared<FVoxelCommittedJournalNode, ESPMode::ThreadSafe>(*InRoot) :
			MakeShared<FVoxelCommittedJournalNode, ESPMode::ThreadSafe>();
		Node->PathLease = InLease;
		if (InDepth == 32)
		{
			Node->Journal = InJournal;
			Node->Revision = InRevision;
			Node->BasePersistedRevision = InPersisted;
		}
		else
		{
			const int32 Child = ChildAt(InSection, InDepth);
			Node->Children[Child] = SetNode(Node->Children[Child], InSection, InDepth + 1,
				InJournal, InRevision, InPersisted, InLease);
		}
		return Node;
	}

	bool VisitDirtyNode(const FNodePtr& InRoot, const FNodePtr& InPersisted,
		TFunctionRef<bool(const FVoxelOverlaySnapshot&)> InVisit)
	{
		if (!InRoot || InRoot == InPersisted) return true;
		if (InRoot->Journal)
		{
			const uint64 Persisted = FMath::Max(InRoot->BasePersistedRevision,
				InPersisted ? InPersisted->Revision : uint64(0));
			if (InRoot->Revision > Persisted && !InVisit(*InRoot->Journal)) return false;
		}
		for (int32 Child = 0; Child < 8; ++Child)
		{
			if (!VisitDirtyNode(InRoot->Children[Child], InPersisted ? InPersisted->Children[Child] : FNodePtr(), InVisit))
				return false;
		}
		return true;
	}

	void VisitBounds(const FNodePtr& InRoot, const int32 InDepth,
		const uint64 InX, const uint64 InY, const uint64 InZ,
		const FIntVector& InMinimum, const FIntVector& InMaximum,
		TFunctionRef<void(const FIntVector&)> InVisit)
	{
		if (!InRoot) return;
		const uint64 Span = 1ull << (32 - InDepth);
		if (InX > Encode(InMaximum.X) || InX + Span <= Encode(InMinimum.X) ||
			InY > Encode(InMaximum.Y) || InY + Span <= Encode(InMinimum.Y) ||
			InZ > Encode(InMaximum.Z) || InZ + Span <= Encode(InMinimum.Z)) return;
		if (InDepth == 32)
		{
			if (InRoot->Journal) InVisit(InRoot->Journal->Section);
			return;
		}
		const uint64 Half = Span / 2;
		for (int32 Child = 0; Child < 8; ++Child)
		{
			VisitBounds(InRoot->Children[Child], InDepth + 1,
				InX + ((Child & 1) ? Half : 0), InY + ((Child & 2) ? Half : 0),
				InZ + ((Child & 4) ? Half : 0), InMinimum, InMaximum, InVisit);
		}
	}
}

bool FVoxelCommittedJournalCapture::VisitDirty(TFunctionRef<bool(const FVoxelOverlaySnapshot&)> InVisit) const
{
	if (CommitCut == PersistedCut) return true;
	return VisitDirtyNode(Root, PersistedRoot, InVisit);
}

bool FVoxelCommittedJournalCapture::PrepareSaveNotifications(const FGuid& InTransactionId)
{
	check(IsInGameThread());
	if (!Root && !PersistedRoot) return true;
	if (SaveNotifications) return !SaveNotifications->bQueued;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (Resources && !Resources->Acquire(
		FVoxelGenerationConsumerKey::Section(WorldEpoch, InTransactionId, 1, {}, 5),
		EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelJournalSaveNotifications>(), Lease)) return false;
	SaveNotifications = Resources ? VoxelBoundedAllocation::MakeWorkerShared<FVoxelJournalSaveNotifications>(MoveTemp(Lease)) :
		TSharedPtr<FVoxelJournalSaveNotifications, ESPMode::ThreadSafe>(new FVoxelJournalSaveNotifications,
			VoxelBoundedAllocation::TWorkerDeleter<FVoxelJournalSaveNotifications>{});
	SaveNotifications->Root = Root;
	SaveNotifications->PersistedRoot = PersistedRoot;
	SaveNotifications->Path[0] = Root.Get();
	SaveNotifications->PersistedPath[0] = PersistedRoot.Get();
	return true;
}

FVoxelCommittedJournal::FVoxelCommittedJournal(const uint64 InWorldEpoch)
	: WorldEpoch(InWorldEpoch)
{
}

FVoxelCommittedJournal::~FVoxelCommittedJournal()
{
	if (!Root && !PersistedRoot && !RetirementPath[0]) return;
	check(!Resources || ShutdownTaskLease);
	FJournalRootReclaimer Reclaimer;
	Reclaimer.Roots[0] = MoveTemp(Root);
	Reclaimer.Roots[1] = MoveTemp(PersistedRoot);
	for (int32 Depth = 0; Depth < 33; ++Depth) Reclaimer.Roots[Depth + 2] = MoveTemp(RetirementPath[Depth]);
	Reclaimer.TaskLease = MoveTemp(ShutdownTaskLease);
	UE::Tasks::Launch(UE_SOURCE_LOCATION, MoveTemp(Reclaimer));
}

TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> FVoxelCommittedJournalCapture::FindDirty(const FIntVector& InSection) const
{
	if (CommitCut == PersistedCut) return nullptr;
	const auto* Node = FindNode(Root, InSection);
	const auto* Saved = FindNode(PersistedRoot, InSection);
	if (!Node || Node->Revision <= FMath::Max(Node->BasePersistedRevision, Saved ? Saved->Revision : uint64(0))) return nullptr;
	return Node->Journal;
}

void FVoxelCommittedJournal::BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources)
{
	if (Resources && Resources.Get() != &InResources.Get())
	{
		check(!Root && !PersistedRoot && !RetirementPath[0]);
		ShutdownTaskLease.Reset();
		ReclaimTaskLease.Reset();
	}
	Resources = InResources;
	EnsureReclaimLeases();
}

bool FVoxelCommittedJournal::EnsureReclaimLeases() const
{
	if (!Resources) return true;
	const uint64 TaskBytes = VoxelBoundedAllocation::TaskBytes<FJournalRootReclaimer>();
	return (ShutdownTaskLease || Resources->Acquire(
		FVoxelGenerationConsumerKey::Section(WorldEpoch, ReclaimOwnerId, 1, {}, 1), EVoxelTerrainResourcePool::Input, TaskBytes, ShutdownTaskLease)) &&
		(ReclaimTaskLease || Resources->Acquire(
			FVoxelGenerationConsumerKey::Section(WorldEpoch, ReclaimOwnerId, 1, {}, 2), EVoxelTerrainResourcePool::Input, TaskBytes, ReclaimTaskLease));
}

bool FVoxelCommittedJournal::PreparePublication(const FIntVector& InSection, const uint64 InRevision,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease) const
{
	return EnsureReclaimLeases() && (!Resources || Resources->Acquire(
		FVoxelGenerationConsumerKey::Section(WorldEpoch, OwnerId, 1, InSection, InRevision + 1),
		EVoxelTerrainResourcePool::Resident, PathBytes(), OutLease));
}

void FVoxelCommittedJournal::Publish(TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> InJournal)
{
	check(InJournal && (!Resources || InJournal->IndexLease));
	Root = SetNode(Root, InJournal->Section, 0, InJournal, InJournal->Revision,
		InJournal->BasePersistedRevision, InJournal->IndexLease);
}

TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> FVoxelCommittedJournal::Find(const FIntVector& InSection) const
{
	const auto* Node = FindNode(Root, InSection);
	return Node ? Node->Journal : nullptr;
}

uint64 FVoxelCommittedJournal::PersistedRevision(const FIntVector& InSection) const
{
	const auto* Node = FindNode(Root, InSection);
	const auto* Saved = FindNode(PersistedRoot, InSection);
	return FMath::Max(Node ? Node->BasePersistedRevision : uint64(0), Saved ? Saved->Revision : uint64(0));
}

FVoxelCommittedJournalCapture FVoxelCommittedJournal::Capture(const uint64 InCommitCut) const
{
	return {nullptr, Root, PersistedRoot, Resources, WorldEpoch, InCommitCut, OwnerId, SavedCut};
}

uint64 FVoxelCommittedJournal::CommittedRevision(const FIntVector& InSection) const
{
	const auto* Node = FindNode(Root, InSection);
	return Node ? Node->Revision : 0;
}

bool FVoxelCommittedJournal::AcceptSave(const FVoxelCommittedJournalCapture& InCapture)
{
	check(IsInGameThread());
	if (!CanAcceptSave(InCapture)) return false;
	if (InCapture.CommitCut > SavedCut && InCapture.SaveNotifications)
	{
		auto& Notifications = InCapture.SaveNotifications;
		Notifications->bQueued = true;
		if (SaveNotificationsTail) SaveNotificationsTail->Next = Notifications;
		else SaveNotificationsHead = Notifications;
		SaveNotificationsTail = Notifications.Get();
	}
	PersistedRoot = InCapture.Root;
	SavedCut = InCapture.CommitCut;
	return true;
}

bool FVoxelCommittedJournal::CanAcceptSave(const FVoxelCommittedJournalCapture& InCapture) const
{
	if (InCapture.WorldEpoch != WorldEpoch || InCapture.OwnerId != OwnerId || InCapture.CommitCut < SavedCut) return false;
	if (InCapture.CommitCut == SavedCut || InCapture.CommitCut == InCapture.PersistedCut || InCapture.Root == InCapture.PersistedRoot) return true;
	const auto& Notifications = InCapture.SaveNotifications;
	return Notifications && !Notifications->bQueued && Notifications->Root == InCapture.Root &&
		Notifications->PersistedRoot == InCapture.PersistedRoot;
}

bool FVoxelCommittedJournal::HasSaveNotifications() const
{
	return SaveNotificationsHead.IsValid();
}

bool FVoxelCommittedJournal::AdvanceSaveNotifications(FIntVector& OutSection, uint64& OutRevision)
{
	check(IsInGameThread());
	OutRevision = 0;
	if (!SaveNotificationsHead) return false;
	auto& Cursor = *SaveNotificationsHead;
	if (Cursor.Depth == INDEX_NONE)
	{
		auto Finished = MoveTemp(SaveNotificationsHead);
		SaveNotificationsHead = MoveTemp(Finished->Next);
		if (!SaveNotificationsHead) SaveNotificationsTail = nullptr;
		return true;
	}
	const auto* Node = Cursor.Path[Cursor.Depth];
	const auto* Saved = Cursor.PersistedPath[Cursor.Depth];
	if (!Node || Node == Saved)
	{
		--Cursor.Depth;
		return true;
	}
	if (Cursor.Depth == 32)
	{
		--Cursor.Depth;
		if (Node->Journal && Node->Revision > FMath::Max(Node->BasePersistedRevision, Saved ? Saved->Revision : uint64(0)))
		{
			OutSection = Node->Journal->Section;
			OutRevision = Node->Revision;
		}
		return true;
	}
	if (Cursor.Child[Cursor.Depth] == 8)
	{
		--Cursor.Depth;
		return true;
	}
	const int32 Child = Cursor.Child[Cursor.Depth]++;
	++Cursor.Depth;
	Cursor.Path[Cursor.Depth] = Node->Children[Child].Get();
	Cursor.PersistedPath[Cursor.Depth] = Saved ? Saved->Children[Child].Get() : nullptr;
	Cursor.Child[Cursor.Depth] = 0;
	return true;
}

bool FVoxelCommittedJournal::MarkPersisted(const FIntVector& InSection, const uint64 InRevision)
{
	const auto* Current = FindNode(Root, InSection);
	if (!Current || InRevision > Current->Revision || InRevision <= PersistedRevision(InSection)) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (!PreparePublication(InSection, InRevision, Lease)) return false;
	PersistedRoot = SetNode(PersistedRoot, InSection, 0, nullptr, InRevision, InRevision, Lease);
	return true;
}

void FVoxelCommittedJournal::Visit(const FVoxelGenerationBounds& InBounds,
	TFunctionRef<void(const FIntVector&)> InVisit) const
{
	if (!InBounds.IsValid()) return;
	const FIntVector Minimum(VoxelGeneration::FloorDivide(InBounds.Min.X, 16),
		VoxelGeneration::FloorDivide(InBounds.Min.Y, 16), VoxelGeneration::FloorDivide(InBounds.Min.Z, 16));
	const FIntVector Maximum(VoxelGeneration::FloorDivide(InBounds.Max.X - 1, 16),
		VoxelGeneration::FloorDivide(InBounds.Max.Y - 1, 16), VoxelGeneration::FloorDivide(InBounds.Max.Z - 1, 16));
	VisitBounds(Root, 0, 0, 0, 0, Minimum, Maximum, InVisit);
}

void FVoxelCommittedJournal::AdvanceRetirement(const int32 InMaximumNodes, const double InDeadline,
	TFunctionRef<bool(const FIntVector&)> InIsResident)
{
	check(IsInGameThread());
	if (!ReclaimTask.IsCompleted()) return;
	ReclaimTask = {};
	if (!EnsureReclaimLeases()) return;
	if (RetirementDepth == INDEX_NONE)
	{
		if (!Root) return;
		RetirementPath[0] = Root;
		RetirementChild[0] = 0;
		RetirementDepth = 0;
	}
	for (int32 Work = 0; Work < InMaximumNodes && FPlatformTime::Seconds() < InDeadline && RetirementDepth >= 0; ++Work)
	{
		FNodePtr Node = RetirementPath[RetirementDepth];
		if (RetirementDepth < 32 && RetirementChild[RetirementDepth] < 8)
		{
			const FNodePtr Child = Node->Children[RetirementChild[RetirementDepth]++];
			if (Child)
			{
				RetirementPath[++RetirementDepth] = Child;
				RetirementChild[RetirementDepth] = 0;
			}
			continue;
		}
		if (Node->Journal)
		{
			const FIntVector Section = Node->Journal->Section;
			const auto* Current = FindNode(Root, Section);
			if (Current && Current->Journal == Node->Journal &&
				(!Node->Journal->Blocks.IsEmpty() || !Node->Journal->Entities.IsEmpty()) && !InIsResident(Section) &&
				PersistedRevision(Section) >= Current->Revision)
			{
				const uint64 Revision = Current->Revision;
				const bool bSameRoot = Root == PersistedRoot;
				TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
				TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SavedLease;
				if (!PreparePublication(Section, Revision, Lease) ||
					(!bSameRoot && !PreparePublication(Section, Revision, SavedLease))) return;
				Root = SetNode(Root, Section, 0, nullptr, Revision, Revision, Lease);
				if (bSameRoot) PersistedRoot = Root;
				else
				{
					const auto* Saved = FindNode(PersistedRoot, Section);
					if (Saved && Saved->Revision <= Revision)
						PersistedRoot = SetNode(PersistedRoot, Section, 0, nullptr, Saved->Revision, Saved->Revision, SavedLease);
				}
			}
		}
		if (RetirementDepth == 0 && Node != Root && Node != PersistedRoot)
		{
			FJournalRootReclaimer Reclaimer;
			Reclaimer.Roots[0] = MoveTemp(RetirementPath[0]);
			Reclaimer.TaskLease = ReclaimTaskLease;
			Node.Reset();
			ReclaimTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, MoveTemp(Reclaimer));
			RetirementDepth = INDEX_NONE;
			return;
		}
		RetirementPath[RetirementDepth].Reset();
		--RetirementDepth;
	}
}

bool FVoxelCommittedJournal::IsRetirementIdle() const
{
	return ReclaimTask.IsCompleted();
}

uint64 FVoxelCommittedJournal::GetPersistedCut() const
{
	return SavedCut;
}
