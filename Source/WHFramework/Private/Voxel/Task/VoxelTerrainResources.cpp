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
	constexpr int32 MaximumTerrainResourceLeases = 1 << 20;
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

uint64 FVoxelTerrainResourceLease::Serial() const
{
	return LeaseSerial;
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
	if (!InLimits.IsValid() || !Entries.IsEmpty())
	{
		return false;
	}
	// 无活租约时才允许更换配置；账本自身的旧容量也必须真实释放。
	Entries.Empty();
	State.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)] -= State.LedgerMetadataBytes;
	State.LedgerMetadataBytes = 0;
	EntryCapacity = 0;
	State.Limits = InLimits;
	AdvanceVersion(true);
	return true;
}

bool FVoxelTerrainResources::PrepareEntries(const int32 InAdditionalEntries, const uint64 InAdditionalInputBytes)
{
	const int64 Required = int64(Entries.Num()) + State.ReservedSplitCount + InAdditionalEntries;
	if (InAdditionalEntries <= 0 || Required > MaximumTerrainResourceLeases) return false;
	if (Required <= EntryCapacity) return CanAdd(EVoxelTerrainResourcePool::Input, InAdditionalInputBytes);
	int32 Capacity = FMath::Max(16, EntryCapacity);
	while (Capacity < Required) Capacity = FMath::Min(MaximumTerrainResourceLeases, Capacity * 2);
	const uint64 Bytes = VoxelGenerationAllocation::Private::SetReserveBytes<TPair<uint64, FEntry>>(Entries, Capacity);
	// 扩容期间旧表和新表并存。先同时授予新表、句柄及调用者输入，再进行 Reserve。
	if (Bytes > MAX_uint64 - InAdditionalInputBytes ||
		!CanAdd(EVoxelTerrainResourcePool::Input, Bytes + InAdditionalInputBytes)) return false;
	uint64& Used = State.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)];
	Used += Bytes;
	Entries.Reserve(Capacity);
	const uint64 ActualBytes = VoxelGenerationAllocation::AllocatedBytes(Entries);
	check(ActualBytes <= Bytes);
	Used -= State.LedgerMetadataBytes + Bytes - ActualBytes;
	State.LedgerMetadataBytes = ActualBytes;
	EntryCapacity = Capacity;
	return true;
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
	if (RequiredEntries > EntryCapacity)
	{
		int32 Capacity = FMath::Max(16, EntryCapacity);
		while (Capacity < RequiredEntries) Capacity = FMath::Min(MaximumTerrainResourceLeases, Capacity * 2);
		const uint64 NewMetadata = VoxelGenerationAllocation::Private::SetReserveBytes<TPair<uint64, FEntry>>(Entries, Capacity);
		if (NewMetadata > MAX_uint64 - MetadataPeak) return false;
		MetadataPeak += NewMetadata;
	}
	uint64 Remaining = State.Limits.InputBytes;
	for (const uint64 Bytes : {MetadataPeak, Handles, InRequiredInputBytes, InPool == EVoxelTerrainResourcePool::Input ? InBytes : uint64(0)})
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
	if (!InAttempt.IsValid() || InBytes == 0 || OutLease || InReservedSplits < 0 ||
		InReservedSplits >= MaximumTerrainResourceLeases || InRequiredLeaseCount < 0 || InPool >= EVoxelTerrainResourcePool::Count)
	{
		return EVoxelTerrainResourceAdmission::InvalidRequest;
	}
	FScopeLock Lock(&Mutex);
	if (!CanFitRequired(InPool, InBytes, InReservedSplits, InRequiredInputBytes, InRequiredLeaseCount))
	{
		return EVoxelTerrainResourceAdmission::CapacityUnachievable;
	}
	const uint64 InputBytes = InPool == EVoxelTerrainResourcePool::Input ? InBytes : 0;
	const uint64 Handles = uint64(InReservedSplits + 1) * TerrainLeaseHandleBytes();
	if (!CanAdd(InPool, InBytes) || InputBytes > MAX_uint64 - Handles ||
		!PrepareEntries(InReservedSplits + 1, InputBytes + Handles))
	{
		return EVoxelTerrainResourceAdmission::CapacityBlocked;
	}
	checkf(NextSerial != MAX_uint64, TEXT("Terrain reservation serial exhausted"));
	const uint64 Serial = ++NextSerial;
	Entries.Add(Serial, {InAttempt, InPool, InBytes, InReservedSplits});
	State.Used[static_cast<int32>(InPool)] += InBytes;
	State.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)] += Handles;
	State.LeaseHandleBytes += Handles;
	++State.LeaseCount;
	State.ReservedSplitCount += InReservedSplits;
	OutLease = MakeShared<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>(AsShared(), Serial);
	AdvanceVersion();
	return EVoxelTerrainResourceAdmission::Granted;
}

bool FVoxelTerrainResources::Resize(const uint64 InSerial, const uint64 InBytes)
{
	return TryResize(InSerial, InBytes, 0, 0) == EVoxelTerrainResourceAdmission::Granted;
}

EVoxelTerrainResourceAdmission FVoxelTerrainResources::TryResize(const uint64 InSerial, const uint64 InBytes,
	const uint64 InRequiredInputBytes, const int32 InRequiredLeaseCount)
{
	FScopeLock Lock(&Mutex);
	FEntry* Entry = Entries.Find(InSerial);
	if (!Entry || InRequiredLeaseCount < 0)
	{
		return EVoxelTerrainResourceAdmission::InvalidRequest;
	}
	if (InBytes > Entry->Bytes)
	{
		if (!CanFitRequired(Entry->Pool, InBytes, Entry->ReservedSplits, InRequiredInputBytes, InRequiredLeaseCount))
			return EVoxelTerrainResourceAdmission::CapacityUnachievable;
		if (!CanAdd(Entry->Pool, InBytes - Entry->Bytes)) return EVoxelTerrainResourceAdmission::CapacityBlocked;
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
	FEntry* Entry = Entries.Find(InSerial);
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
	FEntry* Entry = Entries.Find(InSerial);
	FEntry* Reserved = Entries.Find(InReservedSerial);
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
	FEntry* Parent = Entries.Find(InSerial);
	if (!Parent || InBytes == 0 || InBytes > Parent->Bytes) return nullptr;
	const bool bPrepaid = Parent->ReservedSplits > 0;
	if (!bPrepaid && !PrepareEntries(1, TerrainLeaseHandleBytes())) return nullptr;
	// Reserve 可能移动原表；扩容后重新查找唯一的父租约。
	Parent = Entries.Find(InSerial);
	check(Parent);
	check(NextSerial != MAX_uint64);
	FEntry Child = *Parent;
	Child.Bytes = InBytes;
	Child.ReservedSplits = 0;
	Parent->Bytes -= InBytes;
	if (bPrepaid)
	{
		--Parent->ReservedSplits;
		--State.ReservedSplitCount;
	}
	const uint64 ChildSerial = ++NextSerial;
	Entries.Add(ChildSerial, MoveTemp(Child));
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
	FEntry Entry;
	if (!Entries.RemoveAndCopyValue(InSerial, Entry))
	{
		return;
	}
	uint64& Used = State.Used[static_cast<int32>(Entry.Pool)];
	check(Used >= Entry.Bytes);
	Used -= Entry.Bytes;
	const uint64 Handles = uint64(Entry.ReservedSplits + 1) * TerrainLeaseHandleBytes();
	State.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)] -= Handles;
	State.LeaseHandleBytes -= Handles;
	State.ReservedSplitCount -= Entry.ReservedSplits;
	--State.LeaseCount;
	AdvanceVersion(true, &Entry.Attempt);
}

uint64 FVoxelTerrainResources::Bytes(const uint64 InSerial) const
{
	FScopeLock Lock(&Mutex);
	const FEntry* Entry = Entries.Find(InSerial);
	return Entry ? Entry->Bytes : 0;
}

FVoxelTerrainResourceSnapshot FVoxelTerrainResources::Snapshot() const
{
	FScopeLock Lock(&Mutex);
	return State;
}

EVoxelTerrainResourcePool FVoxelTerrainResources::GetPool(const uint64 InSerial) const
{
	FScopeLock Lock(&Mutex);
	const FEntry* Entry = Entries.Find(InSerial);
	return Entry ? Entry->Pool : EVoxelTerrainResourcePool::Count;
}

uint64 FVoxelTerrainResources::ReleaseVersionExcludingOwner(const FVoxelGenerationConsumerKey& InOwner) const
{
	FScopeLock Lock(&Mutex);
	return LatestRelease.IsOwner(InOwner) ? PreviousRelease.Version : LatestRelease.Version;
}

bool FVoxelTerrainResources::TryGetReleaseVersionExcludingOwner(const FVoxelGenerationConsumerKey& InOwner, uint64& OutVersion) const
{
	if (!Mutex.TryLock()) return false;
	OutVersion = LatestRelease.IsOwner(InOwner) ? PreviousRelease.Version : LatestRelease.Version;
	Mutex.Unlock();
	return true;
}
