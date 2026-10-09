#include "Voxel/Rendering/VoxelFineHandoff.h"

#include "Voxel/Generation/VoxelGenerationAllocation.h"

FVoxelFineHandoffTransaction& FVoxelFineHandoffTransaction::operator=(const FVoxelFineHandoffTransaction& InOther)
{
	if (this == &InOther) return *this;
	Changes.Empty();
	Lease = InOther.Lease;
	Id = InOther.Id;
	WorldEpoch = InOther.WorldEpoch;
	DemandRevision = InOther.DemandRevision;
	Changes.Reserve(InOther.Changes.Num());
	Changes.Append(InOther.Changes);
	return *this;
}

FVoxelFineHandoffTransaction& FVoxelFineHandoffTransaction::operator=(FVoxelFineHandoffTransaction&& InOther)
{
	if (this == &InOther) return *this;
	Changes.Empty();
	Lease = MoveTemp(InOther.Lease);
	Id = InOther.Id;
	WorldEpoch = InOther.WorldEpoch;
	DemandRevision = InOther.DemandRevision;
	Changes = MoveTemp(InOther.Changes);
	return *this;
}

bool FVoxelFineHandoff::BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InControlLease)
{
	if (Resources || !InConsumer.IsValid() || InConsumer.WorldEpoch != WorldEpoch ||
		InResources->GetControlConsumer().WorldEpoch != WorldEpoch ||
		(InControlLease && !InControlLease->IsOwnedBy(InResources.Get()))) return false;
	Resources = MoveTemp(InResources);
	Consumer = InConsumer;
	ControlLease = MoveTemp(InControlLease);
	return true;
}

bool FVoxelFineHandoff::PrepareNativeTables(const int32 InAdditionalChanges,
	const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InPreparationLease, FString& OutError)
{
	if (!Resources)
	{
#if WITH_DEV_AUTOMATION_TESTS
		return true;
#else
		OutError = TEXT("InvalidRequest: unbound native handoff");
		return false;
#endif
	}
	const uint64 PreparationBytes = 3 * VoxelBoundedAllocation::ArrayBytes<FVoxelFineOwnershipChange>(InAdditionalChanges) +
		VoxelBoundedAllocation::SharedBytes<FVoxelFineFrozenHandoff>();
	if (!InPreparationLease || !InPreparationLease->IsOwnedBy(*Resources) || InPreparationLease->Bytes() < PreparationBytes)
	{
		OutError = TEXT("InvalidRequest: unfunded native handoff transaction");
		return false;
	}
	const int64 PresentedCount = int64(PresentedAtoms.Num()) + Locks.Num() + InAdditionalChanges;
	const int64 LockCount = int64(Locks.Num()) + InAdditionalChanges;
	const int64 SettledCount = int64(SettledReceipts.Num()) + Transactions.Num() + 1;
	if (PresentedCount > MAX_int32 || LockCount > MAX_int32 || SettledCount > MAX_int32 || Transactions.Num() == MAX_int32)
	{
		OutError = TEXT("CapacityUnachievable: native handoff directory");
		return false;
	}
	using FPresented = TPair<FIntVector, FVoxelFinePresentedAtom>;
	using FLock = TPair<FIntVector, FGuid>;
	using FTransaction = TPair<FGuid, FTransactionRecord>;
	using FSettled = TPair<FGuid, FVoxelFinePublishStamp>;
	const bool Grow[] = {
		!VoxelGenerationAllocation::Private::SetCanReserve<FPresented>(PresentedAtoms, int32(PresentedCount)),
		!VoxelGenerationAllocation::Private::SetCanReserve<FLock>(Locks, int32(LockCount)),
		!VoxelGenerationAllocation::Private::SetCanReserve<FTransaction>(Transactions, Transactions.Num() + 1),
		!VoxelGenerationAllocation::Private::SetCanReserve<FSettled>(SettledReceipts, int32(SettledCount))};
	if (!Grow[0] && !Grow[1] && !Grow[2] && !Grow[3]) return true;
	const uint64 Old[] = {VoxelGenerationAllocation::AllocatedBytes(PresentedAtoms),
		VoxelGenerationAllocation::AllocatedBytes(Locks), VoxelGenerationAllocation::AllocatedBytes(Transactions),
		VoxelGenerationAllocation::AllocatedBytes(SettledReceipts)};
	const uint64 Next[] = {
		VoxelGenerationAllocation::Private::SetReserveBytes<FPresented>(PresentedAtoms, int32(PresentedCount)),
		VoxelGenerationAllocation::Private::SetReserveBytes<FLock>(Locks, int32(LockCount)),
		VoxelGenerationAllocation::Private::SetReserveBytes<FTransaction>(Transactions, Transactions.Num() + 1),
		VoxelGenerationAllocation::Private::SetReserveBytes<FSettled>(SettledReceipts, int32(SettledCount))};
	uint64 Current = 0;
	for (const uint64 Bytes : Old) Current += Bytes;
	uint64 Peak = Current;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Grow); ++Index)
	{
		if (!Grow[Index]) continue;
		if (Next[Index] == MAX_uint64 || Next[Index] > MAX_uint64 - Current)
		{
			OutError = TEXT("CapacityUnachievable: native handoff allocator");
			return false;
		}
		Peak = FMath::Max(Peak, Current + Next[Index]);
		Current = Current - Old[Index] + Next[Index];
	}
	uint64 RequiredInput = 0;
	int32 RequiredCount = NativeLease ? 1 : 0;
	const FVoxelTerrainResourceLease* Required[2] = {ControlLease.Get(), InPreparationLease.Get()};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Required); ++Index)
	{
		const auto* Lease = Required[Index];
		if (!Lease || (Index && Required[0] && Lease->Serial() == Required[0]->Serial())) continue;
		++RequiredCount;
		if (Lease->GetPool() == EVoxelTerrainResourcePool::Input) RequiredInput += Lease->Bytes();
	}
	const auto Admission = NativeLease ? NativeLease->TryResize(Peak, RequiredInput, RequiredCount) :
		Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, Peak, NativeLease, 0, RequiredInput, RequiredCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: native handoff directory") :
			Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: native handoff directory") :
			TEXT("InvalidRequest: native handoff directory");
		return false;
	}
	if (Grow[0]) PresentedAtoms.Reserve(int32(PresentedCount));
	if (Grow[1]) Locks.Reserve(int32(LockCount));
	if (Grow[2]) Transactions.Reserve(Transactions.Num() + 1);
	if (Grow[3]) SettledReceipts.Reserve(int32(SettledCount));
	verify(NativeLease->Resize(VoxelGenerationAllocation::AllocatedBytes(PresentedAtoms) +
		VoxelGenerationAllocation::AllocatedBytes(Locks) + VoxelGenerationAllocation::AllocatedBytes(Transactions) +
		VoxelGenerationAllocation::AllocatedBytes(SettledReceipts)));
	NativeLease->MarkPublished();
	return true;
}

FVoxelFineHandoff::FVoxelFineHandoff(const uint64 InWorldEpoch)
	: WorldEpoch(InWorldEpoch)
{
	check(WorldEpoch != 0);
}

bool FVoxelFineHandoff::ValidateEvidence(const FVoxelFineOwnerEvidence& InEvidence) const
{
	const bool bFine = InEvidence.Owner.Kind == EVoxelFineOwnerKind::F0 ||
		InEvidence.Owner.Kind == EVoxelFineOwnerKind::F1Plus;
	const bool bProxy = InEvidence.Owner.Kind == EVoxelFineOwnerKind::Proxy;
	const bool bHeightfield = InEvidence.Owner.Kind == EVoxelFineOwnerKind::Surface ||
		InEvidence.Owner.Kind == EVoxelFineOwnerKind::Macro;
	const bool bRetirement = InEvidence.Owner.Kind == EVoxelFineOwnerKind::None && InEvidence.bKnownEmpty;
	const bool bStepValid = bFine ? InEvidence.Content.SampleStepCells == 1 :
		bRetirement ? InEvidence.Content.SampleStepCells == 0 :
		(bProxy || bHeightfield) && InEvidence.Content.SampleStepCells >= (bProxy ? 2 : 1) &&
		FMath::IsPowerOfTwo(InEvidence.Content.SampleStepCells);
	return bStepValid && InEvidence.Owner.Identity.IsValid() && InEvidence.Owner.Incarnation != 0 &&
		InEvidence.Content.WorldEpoch == WorldEpoch && InEvidence.Content.GeneratorIdentity != 0 &&
		InEvidence.Content.GeometryIdentity.IsValid() && InEvidence.bGeometryReady && InEvidence.bCanStage &&
		InEvidence.GeometryLease.IsValid();
}

void FVoxelFineHandoff::SetSectionChangedCallback(TFunction<void(const FIntVector&)> InCallback)
{
	SectionChangedCallback = MoveTemp(InCallback);
}

bool FVoxelFineHandoff::Prepare(TConstArrayView<FVoxelFineOwnershipChange> InChanges, const uint64 InDemandRevision,
	FVoxelFineHandoffTransaction& OutTransaction, FString& OutError,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InPreparationLease)
{
	if (InChanges.IsEmpty() || InChanges.Num() > 512)
	{
		OutError = TEXT("交接事务为空或超过局部事务上限");
		return false;
	}
	for (int32 Index = 0; Index < InChanges.Num(); ++Index)
	{
		const FVoxelFineOwnershipChange& Change = InChanges[Index];
		bool bDuplicate = false;
		for (int32 BeforeIndex = 0; BeforeIndex < Index; ++BeforeIndex) bDuplicate |= InChanges[BeforeIndex].Section == Change.Section;
		const FVoxelFinePresentedAtom* Existing = PresentedAtoms.Find(Change.Section);
		const FVoxelFineOwner Before = Existing ? Existing->Evidence.Owner : FVoxelFineOwner();
		const uint64 BeforeGeneration = Existing ? Existing->OwnerGeneration : 0;
		if (bDuplicate || Locks.Contains(Change.Section) || Before != Change.Before ||
			BeforeGeneration != Change.BeforeGeneration || BeforeGeneration == MAX_uint64 || !ValidateEvidence(Change.After))
		{
			OutError = TEXT("交接存在冲突、过期逐 Atom owner 或不完整的替换证据");
			return false;
		}
	}
	if (!PrepareNativeTables(InChanges.Num(), InPreparationLease, OutError)) return false;
	FVoxelFineHandoffTransaction Candidate;
	Candidate.Lease = MoveTemp(InPreparationLease);
	Candidate.Id = FGuid::NewGuid();
	Candidate.WorldEpoch = WorldEpoch;
	Candidate.DemandRevision = InDemandRevision;
	Candidate.Changes.Reserve(InChanges.Num());
	Candidate.Changes.Append(InChanges.GetData(), InChanges.Num());
	Candidate.Changes.Sort([](const FVoxelFineOwnershipChange& First, const FVoxelFineOwnershipChange& Second)
	{
		if (First.Section.X != Second.Section.X)
		{
			return First.Section.X < Second.Section.X;
		}
		if (First.Section.Y != Second.Section.Y)
		{
			return First.Section.Y < Second.Section.Y;
		}
		return First.Section.Z < Second.Section.Z;
	});
	for (const FVoxelFineOwnershipChange& Change : Candidate.Changes)
	{
		Locks.Add(Change.Section, Candidate.Id);
	}
	FTransactionRecord Record;
	Record.Transaction = Candidate;
	Transactions.Add(Candidate.Id, MoveTemp(Record));
	OutTransaction = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}

bool FVoxelFineHandoff::ValidateBeforeSeal(const FGuid& InTransaction,
	TFunctionRef<bool(const FIntVector&, const FVoxelFineOwnerEvidence&)> InIsEvidenceCurrent,
	TFunctionRef<bool(const FIntVector&, const FVoxelFineOwnerEvidence&)> InIsCollisionCurrent, FString& OutError) const
{
	const FTransactionRecord* Record = Transactions.Find(InTransaction);
	if (!Record || Record->Transaction.WorldEpoch != WorldEpoch || Record->Frozen || Record->bCancelRequested)
	{
		OutError = TEXT("交接事务不处于可封存状态");
		return false;
	}
	for (const FVoxelFineOwnershipChange& Change : Record->Transaction.Changes)
	{
		const FVoxelFinePresentedAtom* Existing = PresentedAtoms.Find(Change.Section);
		const FVoxelFineOwner Before = Existing ? Existing->Evidence.Owner : FVoxelFineOwner();
		const uint64 BeforeGeneration = Existing ? Existing->OwnerGeneration : 0;
		const FGuid* Lock = Locks.Find(Change.Section);
		if (Before != Change.Before || BeforeGeneration != Change.BeforeGeneration || !Lock || *Lock != InTransaction ||
			!ValidateEvidence(Change.After) || !InIsEvidenceCurrent(Change.Section, Change.After) ||
			(Change.bRequiresCollision && !InIsCollisionCurrent(Change.Section, Change.After)))
		{
			OutError = TEXT("封存前输入、owner 或物理证明失效；保留旧显示");
			return false;
		}
	}
	OutError.Reset();
	return true;
}

bool FVoxelFineHandoff::SealTransaction(const FGuid& InTransaction, const FVoxelFinePublishStamp& InStamp,
	TSharedPtr<const FVoxelTaskCustomPayload, ESPMode::ThreadSafe> InPublicationLease,
	TFunctionRef<bool(const FIntVector&, const FVoxelFineOwnerEvidence&)> InIsEvidenceCurrent,
	TFunctionRef<bool(const FIntVector&, const FVoxelFineOwnerEvidence&)> InIsCollisionCurrent,
	TFunctionRef<bool(const FVoxelFineHandoffTransaction&, const FVoxelTaskCustomPayload&)> InAreInterfacesCurrent,
	TSharedPtr<const FVoxelFineFrozenHandoff, ESPMode::ThreadSafe>& OutFrozen, FString& OutError)
{
	if (InStamp.WorldEpoch != WorldEpoch || InStamp.PresentationSerial == 0 ||
		InStamp.PresentationSerial < HighestSealedPresentationSerial || InStamp.FrameCounter < HighestSealedFrameCounter ||
		(InStamp.PresentationSerial == HighestSealedPresentationSerial && InStamp.FrameCounter != HighestSealedFrameCounter) ||
		!InPublicationLease)
	{
		OutError = TEXT("封存需要有效发布代和保持完整界面及渲染资源的租约");
		return false;
	}
	if (!ValidateBeforeSeal(InTransaction, InIsEvidenceCurrent, InIsCollisionCurrent, OutError))
	{
		return false;
	}
	FTransactionRecord& Record = Transactions.FindChecked(InTransaction);
	for (const FVoxelFineOwnershipChange& Change : Record.Transaction.Changes)
	{
		const FVoxelFinePresentedAtom* Existing = PresentedAtoms.Find(Change.Section);
		if (Existing && Existing->Stamp.PresentationSerial >= InStamp.PresentationSerial)
		{
			OutError = TEXT("同一 Atom 的下一次提交必须使用更大的发布代");
			return false;
		}
	}
	if (!InAreInterfacesCurrent(Record.Transaction, *InPublicationLease))
	{
		OutError = TEXT("封存前完整两侧界面输入或局部所有权签名失效");
		return false;
	}
	const auto Frozen = MakeShared<FVoxelFineFrozenHandoff, ESPMode::ThreadSafe>();
	Frozen->Transaction = Record.Transaction;
	Frozen->Stamp = InStamp;
	Frozen->PublicationLease = MoveTemp(InPublicationLease);
	Record.Frozen = Frozen;
	HighestSealedPresentationSerial = InStamp.PresentationSerial;
	HighestSealedFrameCounter = InStamp.FrameCounter;
	OutFrozen = Frozen;
	OutError.Reset();
	return true;
}

EVoxelFineReceiptDisposition FVoxelFineHandoff::CommitRenderReceipt(const FVoxelFineRenderReceipt& InReceipt,
	TArray<FVoxelFinePresentedAtom>& OutRetirement, FString& OutError)
{
	if (InReceipt.Stamp.WorldEpoch != WorldEpoch)
	{
		OutError = TEXT("旧世界回执只能由该世界清理所有者结算");
		return EVoxelFineReceiptDisposition::WorldExpired;
	}
	if (const FVoxelFinePublishStamp* Settled = SettledReceipts.Find(InReceipt.TransactionId))
	{
		if (*Settled == InReceipt.Stamp)
		{
			OutError.Reset();
			return EVoxelFineReceiptDisposition::Duplicate;
		}
		OutError = TEXT("同一事务重复回执的发布身份冲突");
		return EVoxelFineReceiptDisposition::Invalid;
	}
	FTransactionRecord* Record = Transactions.Find(InReceipt.TransactionId);
	if (!Record)
	{
		OutError = TEXT("回执没有对应的在途或尚保留结算记录");
		return EVoxelFineReceiptDisposition::Unknown;
	}
	const TSharedPtr<const FVoxelFineFrozenHandoff, ESPMode::ThreadSafe> Frozen = Record->Frozen;
	if (!Frozen || Frozen->Stamp != InReceipt.Stamp || InReceipt.Atoms.Num() != Frozen->Transaction.Changes.Num())
	{
		OutError = TEXT("回执未匹配完整封存事务");
		return EVoxelFineReceiptDisposition::Invalid;
	}
	for (int32 Index = 0; Index < InReceipt.Atoms.Num(); ++Index)
	{
		const FVoxelFineRenderReceiptAtom& ReceiptAtom = InReceipt.Atoms[Index];
		const FVoxelFineOwnershipChange& Change = Frozen->Transaction.Changes[Index];
		if (ReceiptAtom.Section != Change.Section || ReceiptAtom.Owner != Change.After.Owner ||
			ReceiptAtom.OwnerGeneration != Change.BeforeGeneration + 1 || ReceiptAtom.Content != Change.After.Content ||
			ReceiptAtom.bKnownEmpty != Change.After.bKnownEmpty)
		{
			OutError = TEXT("回执的逐 Atom owner、内容版本或覆盖证明不匹配");
			return EVoxelFineReceiptDisposition::Invalid;
		}
		const FVoxelFinePresentedAtom* Existing = PresentedAtoms.Find(Change.Section);
		const uint64 ExistingGeneration = Existing ? Existing->OwnerGeneration : 0;
		if (ExistingGeneration > ReceiptAtom.OwnerGeneration)
		{
			continue;
		}
		if (ExistingGeneration == ReceiptAtom.OwnerGeneration)
		{
			if (!Existing || Existing->Evidence.Owner != ReceiptAtom.Owner || Existing->Evidence.Content != ReceiptAtom.Content ||
				Existing->Stamp != InReceipt.Stamp || Existing->TransactionId != InReceipt.TransactionId)
			{
				OutError = TEXT("相同逐 Atom owner 代对应不同实际提交");
				return EVoxelFineReceiptDisposition::Invalid;
			}
			continue;
		}
		const FVoxelFineOwner Before = Existing ? Existing->Evidence.Owner : FVoxelFineOwner();
		const FGuid* Lock = Locks.Find(Change.Section);
		if (ExistingGeneration != Change.BeforeGeneration || Before != Change.Before || !Lock || *Lock != InReceipt.TransactionId ||
			(Existing && Existing->Stamp.PresentationSerial >= InReceipt.Stamp.PresentationSerial))
		{
			OutError = TEXT("实际回执存在跳代或逐 Atom 事务锁不一致");
			return EVoxelFineReceiptDisposition::Invalid;
		}
	}
	bool bApplied = false;
	for (int32 Index = 0; Index < InReceipt.Atoms.Num(); ++Index)
	{
		const FVoxelFineRenderReceiptAtom& ReceiptAtom = InReceipt.Atoms[Index];
		const FVoxelFineOwnershipChange& Change = Frozen->Transaction.Changes[Index];
		const FVoxelFinePresentedAtom* Existing = PresentedAtoms.Find(Change.Section);
		if (Existing && Existing->OwnerGeneration >= ReceiptAtom.OwnerGeneration)
		{
			continue;
		}
		if (Existing)
		{
			OutRetirement.Add(*Existing);
		}
		if (Change.After.Owner.Kind == EVoxelFineOwnerKind::None)
		{
			PresentedAtoms.Remove(Change.Section);
			bApplied = true;
			continue;
		}
		FVoxelFinePresentedAtom Presented;
		Presented.Section = Change.Section;
		Presented.Evidence = Change.After;
		Presented.OwnerGeneration = ReceiptAtom.OwnerGeneration;
		Presented.Stamp = InReceipt.Stamp;
		Presented.TransactionId = InReceipt.TransactionId;
		PresentedAtoms.Add(Change.Section, MoveTemp(Presented));
		bApplied = true;
	}
	ReleaseLocks(Frozen->Transaction);
	SettledReceipts.Add(InReceipt.TransactionId, InReceipt.Stamp);
	Transactions.Remove(InReceipt.TransactionId);
	OutError.Reset();
	return bApplied ? EVoxelFineReceiptDisposition::Applied : EVoxelFineReceiptDisposition::Superseded;
}

void FVoxelFineHandoff::ReleaseLocks(const FVoxelFineHandoffTransaction& InTransaction)
{
	for (const FVoxelFineOwnershipChange& Change : InTransaction.Changes)
	{
		const FGuid* Lock = Locks.Find(Change.Section);
		if (Lock && *Lock == InTransaction.Id)
		{
			Locks.Remove(Change.Section);
			if (SectionChangedCallback) SectionChangedCallback(Change.Section);
		}
	}
}

EVoxelFineCancelDisposition FVoxelFineHandoff::RequestCancel(const FGuid& InTransaction,
	TOptional<FVoxelFineHandoffTransaction>& OutCanceledBeforeSeal)
{
	OutCanceledBeforeSeal.Reset();
	FTransactionRecord* Record = Transactions.Find(InTransaction);
	if (!Record)
	{
		return SettledReceipts.Contains(InTransaction) ? EVoxelFineCancelDisposition::AlreadySettled : EVoxelFineCancelDisposition::Unknown;
	}
	if (Record->Frozen)
	{
		Record->bCancelRequested = true;
		return EVoxelFineCancelDisposition::RecordedAfterSeal;
	}
	OutCanceledBeforeSeal = Record->Transaction;
	ReleaseLocks(Record->Transaction);
	Transactions.Remove(InTransaction);
	return EVoxelFineCancelDisposition::CanceledBeforeSeal;
}

const FVoxelFineOwner* FVoxelFineHandoff::FindOwner(const FIntVector& InSection) const
{
	const FVoxelFinePresentedAtom* Presented = PresentedAtoms.Find(InSection);
	return Presented ? &Presented->Evidence.Owner : nullptr;
}

const FVoxelFinePresentedAtom* FVoxelFineHandoff::FindPresentedAtom(const FIntVector& InSection) const
{
	return PresentedAtoms.Find(InSection);
}

bool FVoxelFineHandoff::IsLocked(const FIntVector& InSection) const
{
	return Locks.Contains(InSection);
}

bool FVoxelFineHandoff::CanReleaseF0(const FIntVector& InSection, const bool bInRuntimeDemanded, const bool bInCollisionDemanded,
	const bool bInSimulationDemanded, const bool bInTaskPinned, const bool bInPersistenceDirty,
	TFunctionRef<bool(const FIntVector&, const FVoxelFineOwnerEvidence&)> InIsReplacementCurrent) const
{
	if (bInRuntimeDemanded || bInCollisionDemanded || bInSimulationDemanded || bInTaskPinned || bInPersistenceDirty || Locks.Contains(InSection))
	{
		return false;
	}
	const FVoxelFinePresentedAtom* Presented = PresentedAtoms.Find(InSection);
	return !Presented || (Presented->Evidence.Owner.Kind != EVoxelFineOwnerKind::F0 &&
		InIsReplacementCurrent(InSection, Presented->Evidence));
}

const FVoxelFineHandoffTransaction* FVoxelFineHandoff::FindTransaction(const FGuid& InId) const
{
	const FTransactionRecord* Record = Transactions.Find(InId);
	return Record ? &Record->Transaction : nullptr;
}

bool FVoxelFineHandoff::WasCancelRequested(const FGuid& InId) const
{
	const FTransactionRecord* Record = Transactions.Find(InId);
	return Record && Record->bCancelRequested;
}

void FVoxelFineHandoff::ForgetSettledReceipt(const FGuid& InId)
{
	SettledReceipts.Remove(InId);
}

void FVoxelFineHandoff::DrainForShutdown(const uint64 InNewWorldEpoch, TArray<FVoxelFineHandoffTransaction>& OutPrepared,
	TArray<TSharedPtr<const FVoxelFineFrozenHandoff, ESPMode::ThreadSafe>>& OutFrozen,
	TArray<FVoxelFinePresentedAtom>& OutPresented)
{
	check(InNewWorldEpoch != 0 && InNewWorldEpoch != WorldEpoch);
	SectionChangedCallback = {};
	for (const TPair<FGuid, FTransactionRecord>& Pair : Transactions)
	{
		if (Pair.Value.Frozen)
		{
			OutFrozen.Add(Pair.Value.Frozen);
		}
		else
		{
			OutPrepared.Add(Pair.Value.Transaction);
		}
	}
	for (const TPair<FIntVector, FVoxelFinePresentedAtom>& Pair : PresentedAtoms)
	{
		OutPresented.Add(Pair.Value);
	}
	Transactions.Reset();
	PresentedAtoms.Reset();
	Locks.Reset();
	SettledReceipts.Reset();
	HighestSealedPresentationSerial = 0;
	HighestSealedFrameCounter = 0;
	WorldEpoch = InNewWorldEpoch;
}
