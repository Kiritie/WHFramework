#include "Voxel/Task/VoxelTerrainResources.h"

#include "Misc/ScopeLock.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

namespace
{
	uint64 TerrainLeaseHandleBytes()
	{
		return VoxelBoundedAllocation::SharedBytes<FVoxelTerrainResourceLease>();
	}
	constexpr int32 TerrainResourceSlotBits = 20;
	constexpr int32 MaximumTerrainResourceLeases = 1 << TerrainResourceSlotBits;
}

#if WITH_DEV_AUTOMATION_TESTS
FVoxelTerrainResources::FVoxelTerrainResources()
{
}
#endif

FVoxelTerrainResources::FVoxelTerrainResources(FConstructionKey, const FVoxelTerrainResourceLimits& InLimits,
	const FVoxelGenerationConsumerKey& InControlConsumer, const uint64 InRootInputBytes)
	: ControlConsumer(InControlConsumer)
{
	State.Limits = InLimits;
	State.RootInputBytes = InRootInputBytes;
	State.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)] = InRootInputBytes;
}

FVoxelTerrainResources::~FVoxelTerrainResources()
{
	check(State.LeaseCount == 0);
}

bool FVoxelTerrainResources::Create(const FVoxelTerrainResourceLimits& InLimits,
	const FVoxelGenerationConsumerKey& InControlConsumer,
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& OutResources, FString& OutError)
{
	if (!InLimits.IsValid())
	{
		OutError = TEXT("InvalidResource: resource ledger bootstrap limits");
		return false;
	}
	if (!InControlConsumer.IsValid())
	{
		OutError = TEXT("InvalidConsumer: resource ledger bootstrap control identity");
		return false;
	}
	const uint64 RootBytes = VoxelBoundedAllocation::SharedBytes<FVoxelTerrainResources>();
	if (RootBytes > InLimits.InputBytes)
	{
		OutError = TEXT("Oversized: resource ledger root exceeds its Input limit");
		return false;
	}
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Candidate =
		MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>(FConstructionKey{}, InLimits, InControlConsumer, RootBytes);
	OutResources = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}

bool FVoxelTerrainResourceLimits::IsValid() const
{
	return InputBytes > 0 && WorkingBytes > 0 && StagedBytes > 0 &&
		ResidentBytes > 0 && OverlapBytes > 0 && OverlapBytes < ResidentBytes;
}

FVoxelTerrainResourceLease::FVoxelTerrainResourceLease(
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InOwner, const uint64 InSerial)
	: Owner(InOwner), LeaseSerial(InSerial)
{
}

FVoxelTerrainResourceLease::~FVoxelTerrainResourceLease()
{
	Owner->Release(LeaseSerial);
}

bool FVoxelTerrainResourceLease::Resize(const uint64 InBytes)
{
	return Owner->Resize(LeaseSerial, InBytes);
}

EVoxelTerrainResourceAdmission FVoxelTerrainResourceLease::TryResize(const uint64 InBytes,
	const uint64 InRequiredInputBytes, const int32 InRequiredLeaseCount)
{
	return Owner->TryResize(LeaseSerial, InBytes, InRequiredInputBytes, InRequiredLeaseCount);
}

bool FVoxelTerrainResourceLease::Transfer(const EVoxelTerrainResourcePool InPool)
{
	return Owner->Transfer(LeaseSerial, InPool);
}

bool FVoxelTerrainResourceLease::CommitReservation(
	TSharedRef<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InReserved,
	const FVoxelGenerationConsumerKey& InNewOwner)
{
	if (&Owner.Get() != &InReserved->Owner.Get())
	{
		return false;
	}
	return Owner->CommitReservation(LeaseSerial, InReserved->LeaseSerial, InNewOwner);
}

TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> FVoxelTerrainResourceLease::Split(const uint64 InBytes)
{
	return Owner->Split(LeaseSerial, InBytes);
}

uint64 FVoxelTerrainResourceLease::Bytes() const
{
	return Owner->Bytes(LeaseSerial);
}

EVoxelTerrainResourcePool FVoxelTerrainResourceLease::GetPool() const
{
	return Owner->GetPool(LeaseSerial);
}

bool FVoxelTerrainResourceLease::ReadCapacity(EVoxelTerrainResourcePool& OutPool, uint64& OutBytes) const
{
	return Owner->ReadCapacity(LeaseSerial, OutPool, OutBytes);
}

uint64 FVoxelTerrainResourceLease::Serial() const
{
	return LeaseSerial;
}

void FVoxelTerrainResourceLease::MarkPublished()
{
	Owner->MarkPublished(LeaseSerial);
}

bool FVoxelTerrainResourceLease::IsPublished() const
{
	return Owner->IsPublished(LeaseSerial);
}

bool FVoxelTerrainResources::FReleaseEvent::IsOwner(const FVoxelGenerationConsumerKey& InOwner) const
{
	return Consumer == InOwner;
}

bool FVoxelTerrainResources::FReleaseEvent::IsOwner(const FReleaseEvent& InOther) const
{
	return Consumer == InOther.Consumer;
}

void FVoxelTerrainResources::AdvanceVersion(const bool bInCapacityReleased, const FVoxelGenerationConsumerKey* InReleasingOwner)
{
	checkf(State.Version != MAX_uint64, TEXT("Terrain capacity version exhausted"));
	++State.Version;
	if (State.RequiredResidentHeadroomBytes &&
		CanAdd(EVoxelTerrainResourcePool::Resident, State.RequiredResidentHeadroomBytes))
	{
		State.RequiredResidentHeadroomBytes = 0;
	}
	if (bInCapacityReleased)
	{
		checkf(State.ReleaseVersion != MAX_uint64, TEXT("Terrain release version exhausted"));
		++State.ReleaseVersion;
		const FReleaseEvent Released{InReleasingOwner ? *InReleasingOwner : FVoxelGenerationConsumerKey{}, State.ReleaseVersion};
		if (!LatestRelease.IsOwner(Released)) PreviousRelease = LatestRelease;
		LatestRelease = Released;
	}
}

bool FVoxelTerrainResources::CanAdd(const EVoxelTerrainResourcePool InPool, const uint64 InBytes) const
{
	if (InPool >= EVoxelTerrainResourcePool::Count)
	{
		return false;
	}
	const uint64 Limits[] = {State.Limits.InputBytes, State.Limits.WorkingBytes, State.Limits.StagedBytes,
		State.Limits.ResidentBytes - State.Limits.OverlapBytes, State.Limits.OverlapBytes};
	const int32 Pool = static_cast<int32>(InPool);
	if (State.Used[Pool] > Limits[Pool] || InBytes > Limits[Pool] - State.Used[Pool])
	{
		return false;
	}
	if (InPool == EVoxelTerrainResourcePool::Resident || InPool == EVoxelTerrainResourcePool::ResidentOverlap)
	{
		const uint64 Live = State.Used[static_cast<int32>(EVoxelTerrainResourcePool::Resident)] +
			State.Used[static_cast<int32>(EVoxelTerrainResourcePool::ResidentOverlap)];
		return Live <= State.Limits.ResidentBytes && InBytes <= State.Limits.ResidentBytes - Live;
	}
	return true;
}

bool FVoxelTerrainResources::Configure(const FVoxelTerrainResourceLimits& InLimits)
{
	FScopeLock Lock(&Mutex);
	if (!InLimits.IsValid() || InLimits.InputBytes < State.RootInputBytes || State.LeaseCount != 0)
	{
		return false;
	}
	// 无活租约时才允许更换配置；账本自身的旧容量也必须真实释放。
	EntryPages.Empty();
	FirstFreeEntry = INDEX_NONE;
	State.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)] -= State.LedgerMetadataBytes;
	State.LedgerMetadataBytes = 0;
	EntryCapacity = 0;
	State.Limits = InLimits;
	State.RequiredResidentHeadroomBytes = 0;
	AdvanceVersion(true, ControlConsumer.IsValid() ? &ControlConsumer : nullptr);
	return true;
}

bool FVoxelTerrainResources::CanPrepareEntries(const int32 InAdditionalEntries, const uint64 InAdditionalInputBytes) const
{
	const int64 Required = int64(State.LeaseCount) + State.ReservedSplitCount + InAdditionalEntries;
	if (InAdditionalEntries <= 0 || Required > MaximumTerrainResourceLeases)
	{
		return false;
	}
	const uint64 Bytes = AdditionalLedgerBytes(Required);
	return Bytes <= MAX_uint64 - InAdditionalInputBytes &&
		CanAdd(EVoxelTerrainResourcePool::Input, Bytes + InAdditionalInputBytes);
}

bool FVoxelTerrainResources::PrepareEntries(const int32 InAdditionalEntries, const uint64 InAdditionalInputBytes)
{
	if (!CanPrepareEntries(InAdditionalEntries, InAdditionalInputBytes))
	{
		return false;
	}
	const int64 Required = int64(State.LeaseCount) + State.ReservedSplitCount + InAdditionalEntries;
	if (Required <= EntryCapacity)
	{
		return true;
	}
	const uint64 Bytes = AdditionalLedgerBytes(Required);
	// 只新增固定大小的页；指针目录扩容仍预授予旧、新目录并存的峰值。
	uint64& Used = State.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)];
	Used += Bytes;
	const int32 RequiredPages = int32((Required + FEntryPage::Size - 1) / FEntryPage::Size);
	EntryPages.Reserve(int32(FMath::RoundUpToPowerOfTwo(uint32(RequiredPages))));
	while (EntryPages.Num() < RequiredPages)
	{
		const int32 FirstIndex = EntryPages.Num() * FEntryPage::Size;
		auto Page = MakeUnique<FEntryPage>();
		for (int32 Index = 0; Index < FEntryPage::Size; ++Index)
		{
			Page->Slots[Index].NextFree = FirstFreeEntry;
			FirstFreeEntry = FirstIndex + Index;
		}
		EntryPages.Add(MoveTemp(Page));
	}
	const uint64 ActualBytes = VoxelGenerationAllocation::AllocatedBytes(EntryPages) +
		uint64(EntryPages.Num()) * FMemory::QuantizeSize(sizeof(FEntryPage), alignof(FEntryPage));
	check(ActualBytes <= State.LedgerMetadataBytes + Bytes);
	Used -= State.LedgerMetadataBytes + Bytes - ActualBytes;
	State.LedgerMetadataBytes = ActualBytes;
	EntryCapacity = EntryPages.Num() * FEntryPage::Size;
	return true;
}

uint64 FVoxelTerrainResources::AdditionalLedgerBytes(const int64 InRequiredEntries) const
{
	if (InRequiredEntries <= EntryCapacity)
	{
		return 0;
	}
	const int32 RequiredPages = int32((InRequiredEntries + FEntryPage::Size - 1) / FEntryPage::Size);
	const int32 DirectoryCapacity = int32(FMath::RoundUpToPowerOfTwo(uint32(RequiredPages)));
	const uint64 DirectoryBytes = DirectoryCapacity > EntryPages.Max() ?
		VoxelGenerationAllocation::Private::ArrayReserveBytes<TUniquePtr<FEntryPage>, FDefaultAllocator>(
			DirectoryCapacity) : 0;
	return DirectoryBytes + uint64(RequiredPages - EntryPages.Num()) *
		FMemory::QuantizeSize(sizeof(FEntryPage), alignof(FEntryPage));
}

const FVoxelTerrainResources::FEntry* FVoxelTerrainResources::FindEntry(const uint64 InSerial) const
{
	const int32 Index = int32(InSerial & (MaximumTerrainResourceLeases - 1));
	if (!InSerial || Index >= EntryCapacity)
	{
		return nullptr;
	}
	const FEntrySlot& Slot = EntryPages[Index / FEntryPage::Size]->Slots[Index % FEntryPage::Size];
	return Slot.Serial == InSerial ? &Slot.Entry : nullptr;
}

FVoxelTerrainResources::FEntry* FVoxelTerrainResources::FindEntry(const uint64 InSerial)
{
	return const_cast<FEntry*>(static_cast<const FVoxelTerrainResources*>(this)->FindEntry(InSerial));
}

uint64 FVoxelTerrainResources::AddEntry(const FEntry& InEntry)
{
	check(FirstFreeEntry != INDEX_NONE);
	checkf(NextSerial < (MAX_uint64 >> TerrainResourceSlotBits), TEXT("Terrain reservation serial exhausted"));
	const int32 Index = FirstFreeEntry;
	FEntrySlot& Slot = EntryPages[Index / FEntryPage::Size]->Slots[Index % FEntryPage::Size];
	FirstFreeEntry = Slot.NextFree;
	Slot.Entry = InEntry;
	// 低位定位复用槽，高位保持单调代次，旧句柄不能命中新租约。
	Slot.Serial = (++NextSerial << TerrainResourceSlotBits) | uint64(Index);
	return Slot.Serial;
}

bool FVoxelTerrainResources::Acquire(const FVoxelGenerationConsumerKey& InAttempt,
	const EVoxelTerrainResourcePool InPool, const uint64 InBytes,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease, const int32 InReservedSplits)
{
	return TryAcquire(InAttempt, InPool, InBytes, OutLease, InReservedSplits) == EVoxelTerrainResourceAdmission::Granted;
}

bool FVoxelTerrainResources::CanFitRequired(const EVoxelTerrainResourcePool InPool, const uint64 InBytes,
	const int32 InReservedSplits, const uint64 InRequiredInputBytes, const int32 InRequiredLeaseCount) const
{
	const uint64 Limits[] = {State.Limits.InputBytes, State.Limits.WorkingBytes, State.Limits.StagedBytes,
		State.Limits.ResidentBytes - State.Limits.OverlapBytes, State.Limits.OverlapBytes};
	if (InBytes > Limits[static_cast<int32>(InPool)]) return false;
	const int64 RequiredEntries = int64(InRequiredLeaseCount) + InReservedSplits + 1;
	if (RequiredEntries > MaximumTerrainResourceLeases) return false;
	const uint64 Handles = uint64(RequiredEntries) * TerrainLeaseHandleBytes();
	uint64 MetadataPeak = State.LedgerMetadataBytes;
	const uint64 NewMetadata = AdditionalLedgerBytes(RequiredEntries);
	if (NewMetadata > MAX_uint64 - MetadataPeak) return false;
	MetadataPeak += NewMetadata;
	uint64 Remaining = State.Limits.InputBytes;
	for (const uint64 Bytes : {State.RootInputBytes, MetadataPeak, Handles, InRequiredInputBytes, InPool == EVoxelTerrainResourcePool::Input ? InBytes : uint64(0)})
	{
		if (Bytes > Remaining) return false;
		Remaining -= Bytes;
	}
	return true;
}

EVoxelTerrainResourceAdmission FVoxelTerrainResources::TryAcquire(const FVoxelGenerationConsumerKey& InAttempt,
	const EVoxelTerrainResourcePool InPool, const uint64 InBytes,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease, const int32 InReservedSplits,
	const uint64 InRequiredInputBytes, const int32 InRequiredLeaseCount)
{
	if (!InAttempt.IsValid() || (ControlConsumer.IsValid() && InAttempt.WorldEpoch != ControlConsumer.WorldEpoch) ||
		InBytes == 0 || OutLease || InReservedSplits < 0 ||
		InReservedSplits >= MaximumTerrainResourceLeases || InRequiredLeaseCount < 0 || InPool >= EVoxelTerrainResourcePool::Count)
	{
		return EVoxelTerrainResourceAdmission::InvalidRequest;
	}
	FScopeLock Lock(&Mutex);
	const EVoxelTerrainResourceAdmission Admission = QueryAcquire(InPool, InBytes,
		InReservedSplits, InRequiredInputBytes, InRequiredLeaseCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		if (Admission == EVoxelTerrainResourceAdmission::CapacityBlocked &&
			InPool == EVoxelTerrainResourcePool::Resident && !CanAdd(InPool, InBytes))
		{
			State.RequiredResidentHeadroomBytes = FMath::Max(State.RequiredResidentHeadroomBytes, InBytes);
		}
		return Admission;
	}
	const uint64 InputBytes = InPool == EVoxelTerrainResourcePool::Input ? InBytes : 0;
	const uint64 Handles = uint64(InReservedSplits + 1) * TerrainLeaseHandleBytes();
	if (!PrepareEntries(InReservedSplits + 1, InputBytes + Handles))
	{
		return EVoxelTerrainResourceAdmission::CapacityBlocked;
	}
	const uint64 Serial = AddEntry({InAttempt, InPool, InBytes, InReservedSplits});
	State.Used[static_cast<int32>(InPool)] += InBytes;
	State.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)] += Handles;
	State.LeaseHandleBytes += Handles;
	++State.LeaseCount;
	State.ReservedSplitCount += InReservedSplits;
	OutLease = MakeShared<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>(AsShared(), Serial);
	AdvanceVersion();
	return EVoxelTerrainResourceAdmission::Granted;
}

EVoxelTerrainResourceAdmission FVoxelTerrainResources::QueryAcquire(const EVoxelTerrainResourcePool InPool,
	const uint64 InBytes, const int32 InReservedSplits, const uint64 InRequiredInputBytes,
	const int32 InRequiredLeaseCount) const
{
	if (!CanFitRequired(InPool, InBytes, InReservedSplits, InRequiredInputBytes, InRequiredLeaseCount))
	{
		return EVoxelTerrainResourceAdmission::CapacityUnachievable;
	}
	const uint64 InputBytes = InPool == EVoxelTerrainResourcePool::Input ? InBytes : 0;
	const uint64 Handles = uint64(InReservedSplits + 1) * TerrainLeaseHandleBytes();
	if (!CanAdd(InPool, InBytes) || InputBytes > MAX_uint64 - Handles ||
		!CanPrepareEntries(InReservedSplits + 1, InputBytes + Handles))
	{
		return EVoxelTerrainResourceAdmission::CapacityBlocked;
	}
	return EVoxelTerrainResourceAdmission::Granted;
}

bool FVoxelTerrainResources::TryQueryAcquire(const FVoxelGenerationConsumerKey& InAttempt,
	const EVoxelTerrainResourcePool InPool, const uint64 InBytes,
	EVoxelTerrainResourceAdmission& OutAdmission, const int32 InReservedSplits,
	const uint64 InRequiredInputBytes, const int32 InRequiredLeaseCount) const
{
	OutAdmission = EVoxelTerrainResourceAdmission::None;
	if (!InAttempt.IsValid() || (ControlConsumer.IsValid() && InAttempt.WorldEpoch != ControlConsumer.WorldEpoch) ||
		InBytes == 0 || InReservedSplits < 0 || InReservedSplits >= MaximumTerrainResourceLeases ||
		InRequiredLeaseCount < 0 || InPool >= EVoxelTerrainResourcePool::Count)
	{
		OutAdmission = EVoxelTerrainResourceAdmission::InvalidRequest;
		return true;
	}
	if (!Mutex.TryLock())
	{
		return false;
	}
	OutAdmission = QueryAcquire(InPool, InBytes, InReservedSplits, InRequiredInputBytes, InRequiredLeaseCount);
	Mutex.Unlock();
	return true;
}

bool FVoxelTerrainResources::Resize(const uint64 InSerial, const uint64 InBytes)
{
	return TryResize(InSerial, InBytes, 0, 0) == EVoxelTerrainResourceAdmission::Granted;
}

EVoxelTerrainResourceAdmission FVoxelTerrainResources::TryResize(const uint64 InSerial, const uint64 InBytes,
	const uint64 InRequiredInputBytes, const int32 InRequiredLeaseCount)
{
	FScopeLock Lock(&Mutex);
	FEntry* Entry = FindEntry(InSerial);
	if (!Entry || InRequiredLeaseCount < 0)
	{
		return EVoxelTerrainResourceAdmission::InvalidRequest;
	}
	if (InBytes > Entry->Bytes)
	{
		if (!CanFitRequired(Entry->Pool, InBytes, Entry->ReservedSplits, InRequiredInputBytes, InRequiredLeaseCount))
			return EVoxelTerrainResourceAdmission::CapacityUnachievable;
		if (!CanAdd(Entry->Pool, InBytes - Entry->Bytes))
		{
			if (Entry->Pool == EVoxelTerrainResourcePool::Resident)
			{
				State.RequiredResidentHeadroomBytes = FMath::Max(State.RequiredResidentHeadroomBytes, InBytes - Entry->Bytes);
			}
			return EVoxelTerrainResourceAdmission::CapacityBlocked;
		}
	}
	if (Entry->Bytes != InBytes)
	{
		const bool bReleased = InBytes < Entry->Bytes;
		uint64& Used = State.Used[static_cast<int32>(Entry->Pool)];
		check(Used >= Entry->Bytes);
		Used = Used - Entry->Bytes + InBytes;
		Entry->Bytes = InBytes;
		AdvanceVersion(bReleased, &Entry->Attempt);
	}
	return EVoxelTerrainResourceAdmission::Granted;
}

bool FVoxelTerrainResources::Transfer(const uint64 InSerial, const EVoxelTerrainResourcePool InPool)
{
	FScopeLock Lock(&Mutex);
	FEntry* Entry = FindEntry(InSerial);
	if (!Entry || InPool >= EVoxelTerrainResourcePool::Count)
	{
		return false;
	}
	if (Entry->Pool == InPool)
	{
		return true;
	}
	uint64& PreviousUsed = State.Used[static_cast<int32>(Entry->Pool)];
	check(PreviousUsed >= Entry->Bytes);
	PreviousUsed -= Entry->Bytes;
	if (!CanAdd(InPool, Entry->Bytes))
	{
		PreviousUsed += Entry->Bytes;
		return false;
	}
	State.Used[static_cast<int32>(InPool)] += Entry->Bytes;
	Entry->Pool = InPool;
	AdvanceVersion(true, &Entry->Attempt);
	return true;
}

bool FVoxelTerrainResources::CommitReservation(const uint64 InSerial, const uint64 InReservedSerial,
	const FVoxelGenerationConsumerKey& InNewOwner)
{
	FScopeLock Lock(&Mutex);
	FEntry* Entry = FindEntry(InSerial);
	FEntry* Reserved = FindEntry(InReservedSerial);
	if (!Entry || !Reserved || InSerial == InReservedSerial || !InNewOwner.IsValid() ||
		Entry->Bytes == 0 || Reserved->Bytes < Entry->Bytes || Entry->Pool == Reserved->Pool ||
		!(Entry->Attempt == Reserved->Attempt) || Entry->Attempt.WorldEpoch != InNewOwner.WorldEpoch)
	{
		return false;
	}
	const FVoxelGenerationConsumerKey PreviousOwner = Entry->Attempt;
	uint64& PreviousUsed = State.Used[static_cast<int32>(Entry->Pool)];
	check(PreviousUsed >= Entry->Bytes);
	PreviousUsed -= Entry->Bytes;
	Reserved->Bytes -= Entry->Bytes;
	Entry->Pool = Reserved->Pool;
	Entry->Attempt = InNewOwner;
	// 目标池已含 standby；正文替换它，不重复授额，两个句柄及拆分额度继续各自存活。
	AdvanceVersion(true, &PreviousOwner);
	return true;
}

TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> FVoxelTerrainResources::Split(
	const uint64 InSerial, const uint64 InBytes)
{
	FScopeLock Lock(&Mutex);
	FEntry* Parent = FindEntry(InSerial);
	if (!Parent || InBytes == 0 || InBytes > Parent->Bytes) return nullptr;
	const bool bPrepaid = Parent->ReservedSplits > 0;
	if (!bPrepaid && !PrepareEntries(1, TerrainLeaseHandleBytes())) return nullptr;
	Parent = FindEntry(InSerial);
	check(Parent);
	FEntry Child = *Parent;
	Child.bPublished = false;
	Child.Bytes = InBytes;
	Child.ReservedSplits = 0;
	Parent->Bytes -= InBytes;
	if (bPrepaid)
	{
		--Parent->ReservedSplits;
		--State.ReservedSplitCount;
	}
	const uint64 ChildSerial = AddEntry(Child);
	if (!bPrepaid)
	{
		State.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)] += TerrainLeaseHandleBytes();
		State.LeaseHandleBytes += TerrainLeaseHandleBytes();
	}
	++State.LeaseCount;
	// Payload 总量不变；新句柄和账本节点仍须预授予独立的 Input 容量。
	AdvanceVersion();
	return MakeShared<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>(AsShared(), ChildSerial);
}

void FVoxelTerrainResources::Release(const uint64 InSerial)
{
	FScopeLock Lock(&Mutex);
	const FEntry* Existing = FindEntry(InSerial);
	if (!Existing)
	{
		return;
	}
	const FEntry Entry = *Existing;
	const int32 Index = int32(InSerial & (MaximumTerrainResourceLeases - 1));
	FEntrySlot& Slot = EntryPages[Index / FEntryPage::Size]->Slots[Index % FEntryPage::Size];
	Slot.Entry = {};
	Slot.Serial = 0;
	Slot.NextFree = FirstFreeEntry;
	FirstFreeEntry = Index;
	uint64& Used = State.Used[static_cast<int32>(Entry.Pool)];
	check(Used >= Entry.Bytes);
	Used -= Entry.Bytes;
	const uint64 Handles = uint64(Entry.ReservedSplits + 1) * TerrainLeaseHandleBytes();
	State.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)] -= Handles;
	State.LeaseHandleBytes -= Handles;
	State.ReservedSplitCount -= Entry.ReservedSplits;
	--State.LeaseCount;
	AdvanceVersion(true, &Entry.Attempt);
	// 使用同一release时钟；只有已发布正文最后释放可绕过原owner过滤。
	if (Entry.bPublished)
	{
		State.PublishedRetirementVersion = State.ReleaseVersion;
	}
}

void FVoxelTerrainResources::MarkPublished(const uint64 InSerial)
{
	FScopeLock Lock(&Mutex);
	if (FEntry* Entry = FindEntry(InSerial); Entry && !Entry->bPublished)
	{
		Entry->bPublished = true;
		AdvanceVersion();
	}
}

bool FVoxelTerrainResources::IsPublished(const uint64 InSerial) const
{
	FScopeLock Lock(&Mutex);
	const FEntry* Entry = FindEntry(InSerial);
	return Entry && Entry->bPublished;
}

uint64 FVoxelTerrainResources::Bytes(const uint64 InSerial) const
{
	FScopeLock Lock(&Mutex);
	const FEntry* Entry = FindEntry(InSerial);
	return Entry ? Entry->Bytes : 0;
}

FVoxelTerrainResourceSnapshot FVoxelTerrainResources::Snapshot() const
{
	FScopeLock Lock(&Mutex);
	return State;
}

FVoxelTerrainResourceGrantDiagnostics FVoxelTerrainResources::SnapshotGrants(
	const EVoxelTerrainResourcePool InPool,
	const TArrayView<FVoxelTerrainResourceGrantSnapshot> OutLargest) const
{
	FScopeLock Lock(&Mutex);
	FVoxelTerrainResourceGrantDiagnostics Result;
	Result.Resources = State;
	if (InPool >= EVoxelTerrainResourcePool::Count) return Result;
	const int32 Capacity = FMath::Min(16, OutLargest.Num());
	for (int32 SlotIndex = 0; SlotIndex < EntryCapacity; ++SlotIndex)
	{
		const FEntrySlot& Slot = EntryPages[SlotIndex / FEntryPage::Size]->Slots[SlotIndex % FEntryPage::Size];
		if (!Slot.Serial) continue;
		const FEntry& Entry = Slot.Entry;
		if (Entry.Pool != InPool) continue;
		const int32 Kind = int32(Entry.Attempt.Kind);
		check(Kind >= 0 && Kind < FVoxelTerrainResourceGrantDiagnostics::ConsumerKindCount);
		Result.ConsumerBytes[Kind] += Entry.Bytes;
		++Result.ConsumerLeaseCounts[Kind];
		if (!Capacity || !Entry.Bytes) continue;
		int32 Index = Result.LargestCount;
		while (Index > 0 && (OutLargest[Index - 1].Bytes < Entry.Bytes ||
			(OutLargest[Index - 1].Bytes == Entry.Bytes && OutLargest[Index - 1].Serial > Slot.Serial))) --Index;
		if (Index >= Capacity) continue;
		for (int32 Move = FMath::Min(Result.LargestCount, Capacity - 1); Move > Index; --Move)
			OutLargest[Move] = OutLargest[Move - 1];
		OutLargest[Index] = {Slot.Serial, Entry.Attempt, Entry.Bytes, Entry.bPublished};
		Result.LargestCount = FMath::Min(Result.LargestCount + 1, Capacity);
	}
	return Result;
}

EVoxelTerrainResourcePool FVoxelTerrainResources::GetPool(const uint64 InSerial) const
{
	FScopeLock Lock(&Mutex);
	const FEntry* Entry = FindEntry(InSerial);
	return Entry ? Entry->Pool : EVoxelTerrainResourcePool::Count;
}

bool FVoxelTerrainResources::ReadCapacity(const uint64 InSerial, EVoxelTerrainResourcePool& OutPool, uint64& OutBytes) const
{
	FScopeLock Lock(&Mutex);
	const FEntry* Entry = FindEntry(InSerial);
	if (!Entry) return false;
	OutPool = Entry->Pool;
	OutBytes = Entry->Bytes;
	return true;
}

uint64 FVoxelTerrainResources::ReleaseVersionExcludingOwner(const FVoxelGenerationConsumerKey& InOwner) const
{
	FScopeLock Lock(&Mutex);
	const uint64 External = LatestRelease.IsOwner(InOwner) ? PreviousRelease.Version : LatestRelease.Version;
	return FMath::Max(External, State.PublishedRetirementVersion);
}

bool FVoxelTerrainResources::TryGetReleaseVersionExcludingOwner(const FVoxelGenerationConsumerKey& InOwner, uint64& OutVersion) const
{
	if (!Mutex.TryLock()) return false;
	const uint64 External = LatestRelease.IsOwner(InOwner) ? PreviousRelease.Version : LatestRelease.Version;
	OutVersion = FMath::Max(External, State.PublishedRetirementVersion);
	Mutex.Unlock();
	return true;
}
