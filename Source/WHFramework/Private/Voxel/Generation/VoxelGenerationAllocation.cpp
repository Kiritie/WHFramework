#include "Voxel/Generation/VoxelGenerationAllocation.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "HAL/PlatformTime.h"

FVoxelGenerationAllocationScope*& FVoxelGenerationAllocationScope::ActiveScopeSlot()
{
	static thread_local FVoxelGenerationAllocationScope* Scope = nullptr;
	return Scope;
}

FVoxelGenerationWorkspace*& FVoxelGenerationWorkspace::ActiveWorkspaceSlot()
{
	static thread_local FVoxelGenerationWorkspace* Workspace = nullptr;
	return Workspace;
}

FVoxelGenerationRequiredSourceScope*& FVoxelGenerationRequiredSourceScope::ActiveScopeSlot()
{
	static thread_local FVoxelGenerationRequiredSourceScope* Scope = nullptr;
	return Scope;
}

namespace
{
	// 64位双射只用于本次计量的索引分布；相等判断仍使用原serial/object完整身份。
	uint64 GenerationSourceHash(uint64 Value)
	{
		Value ^= Value >> 33;
		Value *= 0xff51afd7ed558ccdull;
		Value ^= Value >> 33;
		Value *= 0xc4ceb9fe1a85ec53ull;
		return Value ^ (Value >> 33);
	}

	uint64 GenerationResidentLimit(const FVoxelTerrainResourceLimits& InLimits)
	{
		return InLimits.ResidentBytes - InLimits.OverlapBytes;
	}
}

FVoxelGenerationRequiredSourceScope::FVoxelGenerationRequiredSourceScope(
	const FVoxelTerrainResources* InResources,
	const void* InContext,
	FEnumerate InEnumerate,
	const TAtomic<bool>* InCancel)
	: Previous(ActiveScopeSlot())
	, Resources(InContext && InEnumerate ? InResources : nullptr)
	, Context(InContext)
	, Enumerator(InEnumerate)
	, Cancellation(InCancel)
{
	ActiveScopeSlot() = this;
}

FVoxelGenerationRequiredSourceScope::FVoxelGenerationRequiredSourceScope(
	const FVoxelTerrainResources* InResources,
	const FVoxelTerrainResourceLease* InLease)
	: FVoxelGenerationRequiredSourceScope(
		InResources && InLease && InLease->IsOwnedBy(*InResources) ? InResources : nullptr,
		InResources && InLease && InLease->IsOwnedBy(*InResources) ? InLease : nullptr,
		[](const void* InContext, FVisit InVisit)
		{
			VisitLease(static_cast<const FVoxelTerrainResourceLease*>(InContext), InVisit);
		})
{
}

FVoxelGenerationRequiredSourceScope::~FVoxelGenerationRequiredSourceScope()
{
	check(ActiveScopeSlot() == this);
	ActiveScopeSlot() = Previous;
}

void FVoxelGenerationRequiredSourceScope::VisitLease(const FVoxelTerrainResourceLease* InLease, FVisit InVisit)
{
	if (InLease)
	{
		InVisit({nullptr, InLease->Serial(), EVoxelTerrainResourcePool::Input, 0, 1, InLease});
	}
}

const TAtomic<bool>* FVoxelGenerationRequiredSourceScope::ActiveCancellation()
{
	for (auto* Scope = ActiveScopeSlot(); Scope; Scope = Scope->Previous)
		if (Scope->Cancellation) return Scope->Cancellation;
	return nullptr;
}

void FVoxelGenerationRequiredSourceScope::Enumerate(const FVoxelTerrainResources& InResources, FVisit InVisit)
{
	const auto* Cancel = ActiveCancellation();
	for (auto* Scope = ActiveScopeSlot(); Scope; Scope = Scope->Previous)
	{
		if (Cancel && Cancel->Load()) return;
		if (Scope->Resources == &InResources && Scope->Enumerator)
		{
			Scope->Enumerator(Scope->Context, InVisit);
		}
	}
	for (auto* Scope = FVoxelGenerationAllocationScope::ActiveScopeSlot(); Scope; Scope = Scope->Previous)
	{
		if (Scope->Resources.Get() == &InResources)
		{
			VisitLease(Scope->WorkingLease.Get(), InVisit);
			VisitLease(Scope->ResidentReservation.Get(), InVisit);
		}
	}
	for (auto* Workspace = FVoxelGenerationWorkspace::ActiveWorkspaceSlot(); Workspace; Workspace = Workspace->PreviousWorkspace)
	{
		if (Workspace->Resources.Get() == &InResources)
		{
			VisitLease(Workspace->Lease.Get(), InVisit);
		}
	}
}

bool FVoxelGenerationRequiredSourceScope::Collect(
	const FVoxelTerrainResources& InResources,
	const FVoxelTerrainResourceLease* InExcludedLease,
	FVoxelGenerationRequiredCapacity& OutCapacity,
	FString& OutError,
	TArrayView<FVoxelGenerationSourceIdentity> InIdentities)
{
	return MeasureSources([&InResources](FVisit InVisit) { Enumerate(InResources, InVisit); },
		InExcludedLease ? InExcludedLease->Serial() : 0, OutCapacity, OutError, InIdentities);
}

bool FVoxelGenerationRequiredSourceScope::Measure(const void* InContext, FEnumerate InEnumerate,
	FVoxelGenerationRequiredCapacity& OutCapacity, FString& OutError)
{
	return MeasureSources([InContext, InEnumerate](FVisit InVisit) { InEnumerate(InContext, InVisit); },
		0, OutCapacity, OutError);
}

bool FVoxelGenerationRequiredSourceScope::MeasureSources(TFunctionRef<void(FVisit)> InEnumerate,
	const uint64 InExcludedSerial, FVoxelGenerationRequiredCapacity& OutCapacity, FString& OutError,
	TArrayView<FVoxelGenerationSourceIdentity> InIdentities)
{
#if CPUPROFILERTRACE_ENABLED
	static thread_local uint32 MeasureSampleIndex = 0;
	TRACE_CPUPROFILER_EVENT_SCOPE_CONDITIONAL(Voxel_SourceCapacityMeasureSample, (++MeasureSampleIndex & 255) == 0);
#endif
	const auto* Cancel = ActiveCancellation();
	if (Cancel && Cancel->Load()) { OutError = TEXT("Canceled: source capacity measurement"); return false; }
	bool bCanceled = false;
	FVoxelGenerationSourceIdentity StackIdentities[1024];
	const auto Identities = InIdentities.IsEmpty() ? MakeArrayView(StackIdentities) : InIdentities;
	const uint32 SlotCount = uint32(Identities.Num());
	check(SlotCount >= 1024 && FMath::IsPowerOfTwo(SlotCount));
	const uint32 MaximumTracked = SlotCount / 2;
	struct FPartition
	{
		uint64 Prefix = 0;
		uint8 Bits = 0;
		// 0 同次遍历处理两类身份，只有溢出时才分开重放。
		uint8 Kind = 0;
	};
	// 常见情况只遍历一次；表满后按 serial / object 与身份位分区重放。
	// 不再为每个未记录的来源重放整条强引用链；来源数量及精确 union 不变。
	FPartition Pending[66] = {};
	int32 PendingCount = 1;
	FVoxelGenerationRequiredCapacity Capacity;
	bool bValid = true;
	uint64 References = 0, Probes = 0;
	uint32 Passes = 0;
	const double Started = FPlatformTime::Seconds();
	double NextDiagnostic = Started + 5.0;
	while (PendingCount > 0 && bValid && !bCanceled)
	{
		++Passes;
		const FPartition Partition = Pending[--PendingCount];
		// 本地表的成员已默认清零；外部缓冲和后续分区仍完整重置。
		if (!InIdentities.IsEmpty() || Passes > 1)
		{
			FMemory::Memzero(Identities.GetData(), Identities.Num() * sizeof(FVoxelGenerationSourceIdentity));
		}
		uint32 Tracked = 0;
		bool bOverflow = false;
		FVoxelGenerationRequiredCapacity PartCapacity;
		const uint64 Mask = Partition.Bits == 64 ? MAX_uint64 : (uint64(1) << Partition.Bits) - 1;
		InEnumerate([&](const FVoxelGenerationRequiredSource& InSource)
		{
			if (bCanceled) return;
			if (Cancel && (References & 63) == 0 && Cancel->Load()) { bCanceled = true; return; }
			if ((++References & 65535) == 0)
			{
				const double Now = FPlatformTime::Seconds();
				if (Now >= NextDiagnostic)
				{
					NextDiagnostic = Now + 5.0;
					UE_LOG(LogTemp, Warning, TEXT("Voxel source measurement slow: seconds=%.3f references=%llu probes=%llu passes=%u bits=%u kind=%u tracked=%u slots=%u pending=%d"),
						Now - Started, References, Probes, Passes, uint32(Partition.Bits), uint32(Partition.Kind), Tracked, SlotCount, PendingCount);
				}
			}
			if (!bValid || (InSource.Serial && InSource.Serial == InExcludedSerial)) return;
			if ((!InSource.Serial && !InSource.Object) || InSource.Pool >= EVoxelTerrainResourcePool::Count || InSource.LeaseCount < 0)
			{
				bValid = false;
				return;
			}
			if (Partition.Kind && bool(InSource.Serial) != (Partition.Kind == 1)) return;
			const uint64 Key = InSource.Serial ? InSource.Serial : uint64(reinterpret_cast<UPTRINT>(InSource.Object));
			const uint64 Hash = GenerationSourceHash(Key ^ (InSource.Serial ? 0x53455249414cull : 0x4f424a454354ull));
			if ((Hash & Mask) != Partition.Prefix) return;
			// 再混合后取槽位，避免分区固定的低位把连续serial挤到相同探测链。
			uint32 Slot = uint32(GenerationSourceHash(Hash ^ 0x9e3779b97f4a7c15ull)) & (SlotCount - 1);
			for (uint32 Probe = 0; Probe < SlotCount; ++Probe, Slot = (Slot + 1) & (SlotCount - 1))
			{
				++Probes;
				auto& Identity = Identities[Slot];
				if (!Identity.Serial && !Identity.Object)
				{
					if (Tracked == MaximumTracked) { bOverflow = true; return; }
					Identity = {InSource.Serial ? nullptr : InSource.Object, InSource.Serial};
					++Tracked;
					break;
				}
				if (InSource.Serial ? Identity.Serial == InSource.Serial : !Identity.Serial && Identity.Object == InSource.Object) return;
			}
			EVoxelTerrainResourcePool Pool = InSource.Pool;
			uint64 SourceBytes = InSource.Bytes;
			if ((InSource.Lease && !InSource.Lease->ReadCapacity(Pool, SourceBytes)) || Pool >= EVoxelTerrainResourcePool::Count)
			{
				bValid = false;
				return;
			}
			uint64& Bytes = PartCapacity.Bytes[static_cast<int32>(Pool)];
			if (SourceBytes > MAX_uint64 - Bytes || InSource.LeaseCount > MAX_int32 - PartCapacity.LeaseCount)
			{
				bValid = false;
				return;
			}
			Bytes += SourceBytes;
			PartCapacity.LeaseCount += InSource.LeaseCount;
		});
		if (!bValid || bCanceled) break;
		if (bOverflow)
		{
			if (!Partition.Kind)
			{
				Pending[PendingCount++] = {0, 0, 1};
				Pending[PendingCount++] = {0, 0, 2};
				continue;
			}
			check(Partition.Bits < 64 && PendingCount + 2 <= UE_ARRAY_COUNT(Pending));
			Pending[PendingCount++] = {Partition.Prefix | (uint64(1) << Partition.Bits), uint8(Partition.Bits + 1), Partition.Kind};
			Pending[PendingCount++] = {Partition.Prefix, uint8(Partition.Bits + 1), Partition.Kind};
			continue;
		}
		for (int32 Pool = 0; Pool < int32(EVoxelTerrainResourcePool::Count); ++Pool)
		{
			if (PartCapacity.Bytes[Pool] > MAX_uint64 - Capacity.Bytes[Pool])
			{
				bValid = false;
				break;
			}
			Capacity.Bytes[Pool] += PartCapacity.Bytes[Pool];
		}
		if (PartCapacity.LeaseCount > MAX_int32 - Capacity.LeaseCount)
		{
			bValid = false;
			break;
		}
		Capacity.LeaseCount += PartCapacity.LeaseCount;
	}
	if (bCanceled || (Cancel && Cancel->Load()))
	{
		OutError = TEXT("Canceled: source capacity measurement");
		return false;
	}
	if (!bValid)
	{
		OutError = TEXT("CapacityUnachievable: original generation source capacity overflow or inactive lease");
		return false;
	}
	OutCapacity = Capacity;
	return true;
}

FVoxelGenerationAllocationScope::FVoxelGenerationAllocationScope(
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InWorkingLease,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InResidentReservation,
	const uint64 InRootBytes,
	const uint64 InRetainedBytes)
	: Previous(ActiveScopeSlot())
	, Resources(MoveTemp(InResources))
	, Consumer(InConsumer)
	, WorkingLease(MoveTemp(InWorkingLease))
	, ResidentReservation(MoveTemp(InResidentReservation))
	, RootBytes(InRootBytes)
{
	ActiveScopeSlot() = this;
	if (Resources && (!Consumer.IsValid() ||
		(Resources->GetControlConsumer().IsValid() && Consumer.WorldEpoch != Resources->GetControlConsumer().WorldEpoch) ||
		!WorkingLease || !ResidentReservation || RootBytes == 0 ||
		!WorkingLease->IsOwnedBy(*Resources) || !ResidentReservation->IsOwnedBy(*Resources) ||
		WorkingLease->GetPool() != EVoxelTerrainResourcePool::Working ||
		ResidentReservation->GetPool() != EVoxelTerrainResourcePool::Resident ||
		InRetainedBytes > MAX_uint64 - RootBytes ||
		WorkingLease->Bytes() != RootBytes + InRetainedBytes ||
		ResidentReservation->Bytes() != RootBytes + InRetainedBytes))
	{
		FCString::Strncpy(Error, TEXT("InvalidAllocationScope: native body requires original complete reservations"), UE_ARRAY_COUNT(Error));
	}
}

FVoxelGenerationAllocationScope::~FVoxelGenerationAllocationScope()
{
	check(ActiveScopeSlot() == this);
	ActiveScopeSlot() = Previous;
}

FVoxelGenerationAllocationScope* FVoxelGenerationAllocationScope::Get()
{
	return ActiveScopeSlot();
}

bool FVoxelGenerationAllocationScope::AllowPassive(FString& OutError)
{
#if WITH_DEV_AUTOMATION_TESTS
	(void)OutError;
	return true;
#else
	OutError = TEXT("InvalidAllocationScope: native generation requires a bound allocation scope");
	return false;
#endif
}

bool FVoxelGenerationAllocationScope::IsBound() const
{
	return Resources.IsValid();
}

bool FVoxelGenerationAllocationScope::Check(FString& OutError) const
{
	if (Error[0])
	{
		OutError = Error;
		return false;
	}
	return Resources ? true : AllowPassive(OutError);
}

bool FVoxelGenerationAllocationScope::Fail(const TCHAR* InError, FString& OutError)
{
	if (!Error[0])
	{
		FCString::Strncpy(Error, InError, UE_ARRAY_COUNT(Error));
	}
	OutError = Error;
	return false;
}

bool FVoxelGenerationAllocationScope::PrepareBytes(
	const uint64 InOldBytes, const uint64 InNewBytes, FString& OutError)
{
	if (!Check(OutError))
	{
		return false;
	}
	if (!Resources)
	{
		return true;
	}
	const uint64 Current = WorkingLease->Bytes();
	if (Current < RootBytes || InOldBytes > Current - RootBytes || ResidentReservation->Bytes() != Current)
	{
		return Fail(TEXT("AllocationAccountingMismatch: retained backing is not covered by the original body"), OutError);
	}
	if (InNewBytes > MAX_uint64 - Current)
	{
		return Fail(TEXT("Oversized: retained allocation byte overflow"), OutError);
	}
	const uint64 Peak = Current + InNewBytes;
	const uint64 Final = Current - InOldBytes + InNewBytes;
	const FVoxelTerrainResourceLimits Limits = Resources->Snapshot().Limits;
	FVoxelGenerationRequiredCapacity ResidentSources;
	FVoxelGenerationRequiredCapacity WorkingSources;
	if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, ResidentReservation.Get(), ResidentSources, OutError) ||
		!FVoxelGenerationRequiredSourceScope::Collect(*Resources, WorkingLease.Get(), WorkingSources, OutError))
	{
		return Fail(*OutError, OutError);
	}
	const uint64 OtherWorking = WorkingSources.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Working)];
	const uint64 OtherResident = ResidentSources.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Resident)];
	if (OtherWorking > Limits.WorkingBytes || Peak > Limits.WorkingBytes - OtherWorking ||
		OtherResident > GenerationResidentLimit(Limits) || Final > GenerationResidentLimit(Limits) - OtherResident)
	{
		TCHAR CapacityError[512];
		FCString::Snprintf(CapacityError, UE_ARRAY_COUNT(CapacityError),
			TEXT("Oversized: retained body or allocation peak exceeds original pool limit: current=%llu old=%llu new=%llu peak=%llu final=%llu otherWorking=%llu workingLimit=%llu otherResident=%llu residentLimit=%llu"),
			Current, InOldBytes, InNewBytes, Peak, Final, OtherWorking, Limits.WorkingBytes,
			OtherResident, GenerationResidentLimit(Limits));
		return Fail(CapacityError, OutError);
	}
	const auto ResidentAdmission = ResidentReservation->TryResize(Final,
		ResidentSources.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], ResidentSources.LeaseCount);
	if (ResidentAdmission != EVoxelTerrainResourceAdmission::Granted)
	{
		return Fail(ResidentAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable
			? TEXT("CapacityUnachievable: native retained resident standby and original sources")
			: TEXT("CapacityBlocked: native retained resident standby"), OutError);
	}
	const auto WorkingAdmission = WorkingLease->TryResize(Peak,
		WorkingSources.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], WorkingSources.LeaseCount);
	if (WorkingAdmission != EVoxelTerrainResourceAdmission::Granted)
	{
		const bool bRestored = ResidentReservation->Resize(Current);
		check(bRestored);
		return Fail(WorkingAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable
			? TEXT("CapacityUnachievable: native retained working peak and original sources")
			: TEXT("CapacityBlocked: native retained working allocation peak"), OutError);
	}
	return true;
}

bool FVoxelGenerationAllocationScope::SettleBytes(
	const uint64 InOldBytes, const uint64 InReservedBytes, const uint64 InActualBytes, FString& OutError)
{
	if (!Resources)
	{
		return true;
	}
	const uint64 Current = WorkingLease->Bytes();
	if (InActualBytes > InReservedBytes || InReservedBytes > Current ||
		InOldBytes > Current - InReservedBytes || Current - InReservedBytes - InOldBytes < RootBytes)
	{
		return Fail(TEXT("AllocationAccountingMismatch: allocator exceeded pregranted retained backing"), OutError);
	}
	const uint64 Final = Current - InReservedBytes - InOldBytes + InActualBytes;
	// Reserve 已释放原 backing；现在才归还旧块及保守估值余量。
	const bool bWorkingSettled = WorkingLease->Resize(Final);
	const bool bResidentSettled = ResidentReservation->Resize(Final);
	check(bWorkingSettled && bResidentSettled);
	return true;
}

void FVoxelGenerationAllocationScope::ReleaseBytes(const uint64 InBytes)
{
	if (!Resources || InBytes == 0)
	{
		return;
	}
	const uint64 Current = WorkingLease->Bytes();
	check(Current >= RootBytes && InBytes <= Current - RootBytes);
	const bool bWorkingSettled = WorkingLease->Resize(Current - InBytes);
	const bool bResidentSettled = ResidentReservation->Resize(ResidentReservation->Bytes() - InBytes);
	check(bWorkingSettled && bResidentSettled);
}

void FVoxelGenerationAllocationScope::ReleaseRetainedBytes(const uint64 InBytes)
{
	ReleaseBytes(InBytes);
}

bool FVoxelGenerationAllocationScope::ValidateRetainedBytes(const uint64 InActualBytes, FString& OutError)
{
	if (!Check(OutError))
	{
		return false;
	}
	if (!Resources)
	{
		return true;
	}
	if (InActualBytes > MAX_uint64 - RootBytes ||
		WorkingLease->Bytes() != RootBytes + InActualBytes ||
		ResidentReservation->Bytes() != RootBytes + InActualBytes)
	{
		return Fail(TEXT("AllocationAccountingMismatch: native body backing differs from its pregrant"), OutError);
	}
	return true;
}

VoxelGenerationAllocation::Private::FSetCapacityArchive::FSetCapacityArchive()
{
}

VoxelGenerationAllocation::Private::FSetCapacityArchive::~FSetCapacityArchive()
{
}

void VoxelGenerationAllocation::Private::FSetCapacityArchive::CountBytes(const SIZE_T InNum, const SIZE_T InMax)
{
	if (Count < UE_ARRAY_COUNT(MaxBytes))
	{
		MaxBytes[Count] = InMax;
	}
	++Count;
}

FVoxelGenerationWorkspace::FVoxelGenerationWorkspace()
	: PreviousWorkspace(ActiveWorkspaceSlot())
	, Scope(FVoxelGenerationAllocationScope::Get())
{
	ActiveWorkspaceSlot() = this;
	if (Scope)
	{
		Resources = Scope->Resources;
		Consumer = Scope->Consumer;
	}
}

FVoxelGenerationWorkspace::FVoxelGenerationWorkspace(TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer, const uint64 InRequiredInputBytes, const int32 InRequiredLeaseCount,
	const uint64 InRequiredWorkingBytes)
	: PreviousWorkspace(ActiveWorkspaceSlot()), Resources(MoveTemp(InResources)), Consumer(InConsumer), RequiredInputBytes(InRequiredInputBytes), RequiredLeaseCount(InRequiredLeaseCount),
RequiredWorkingBytes(InRequiredWorkingBytes)
{
	ActiveWorkspaceSlot() = this;
}

FVoxelGenerationWorkspace::FVoxelGenerationWorkspace(const FVoxelGenerationWorkspace* InSource)
	: FVoxelGenerationWorkspace()
{
	if (InSource)
	{
		Scope = InSource->Scope;
		Resources = InSource->Resources;
		Consumer = InSource->Consumer;
		RequiredInputBytes = InSource->GetRequiredInputBytes();
		RequiredLeaseCount = InSource->GetRequiredLeaseCount();
		RequiredWorkingBytes = InSource->GetRequiredWorkingBytes();
	}
}

FVoxelGenerationWorkspace::~FVoxelGenerationWorkspace()
{
	// 受管容器按声明顺序已经销毁；原 Working 租约在此生命周期之后退出。
	check(ActiveWorkspaceSlot() == this);
	ActiveWorkspaceSlot() = PreviousWorkspace;
}

bool FVoxelGenerationWorkspace::Check(FString& OutError) const
{
	if (Scope && Scope != FVoxelGenerationAllocationScope::Get())
	{
		return Scope->Fail(TEXT("InvalidAllocationScope: workspace used outside its original body scope"), OutError);
	}
	if (Scope) return Scope->Check(OutError);
	if (Resources && (!Consumer.IsValid() ||
		(Resources->GetControlConsumer().IsValid() && Consumer.WorldEpoch != Resources->GetControlConsumer().WorldEpoch)))
	{
		OutError = TEXT("InvalidConsumer: standalone workspace requires original complete identity");
		return false;
	}
	return Resources ? true : FVoxelGenerationAllocationScope::AllowPassive(OutError);
}

bool FVoxelGenerationWorkspace::Fail(const TCHAR* InError, FString& OutError)
{
	if (Scope)
	{
		return Scope->Fail(InError, OutError);
	}
	OutError = InError;
	return false;
}

bool FVoxelGenerationWorkspace::PrepareBytes(
	const uint64 InOldBytes, const uint64 InNewBytes, FString& OutError)
{
	if (!Check(OutError))
	{
		return false;
	}
	if (!Resources)
	{
		return true;
	}
	const uint64 Current = Lease ? Lease->Bytes() : 0;
	if (InOldBytes > Current)
	{
		return Fail(*FString::Printf(TEXT("AllocationAccountingMismatch: scratch backing is not covered by its workspace (old=%llu workspace=%llu requested=%llu)"),
			InOldBytes, Current, InNewBytes), OutError);
	}
	const uint64 WorkingLimit = Resources->Snapshot().Limits.WorkingBytes;
	FVoxelGenerationRequiredCapacity Sources;
	if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, Lease.Get(), Sources, OutError))
	{
		return Fail(*OutError, OutError);
	}
	const uint64 RequiredWorking = FMath::Max(RequiredWorkingBytes,
		Sources.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Working)]);
	const uint64 RequiredInput = FMath::Max(RequiredInputBytes,
		Sources.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)]);
	const int32 RequiredCount = FMath::Max(RequiredLeaseCount, Sources.LeaseCount);
	if (RequiredWorking > WorkingLimit || Current > WorkingLimit - RequiredWorking ||
		InNewBytes > WorkingLimit - RequiredWorking - Current)
	{
		return Fail(TEXT("Oversized: native workspace allocation peak exceeds original pool limit"), OutError);
	}
	const uint64 Peak = Current + InNewBytes;
	if (Peak == 0)
	{
		return true;
	}
	const auto Admission = Lease ? Lease->TryResize(Peak, RequiredInput, RequiredCount) :
		Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Working, Peak, Lease, 0, RequiredInput, RequiredCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		return Fail(Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable
			? TEXT("CapacityUnachievable: native workspace and required input")
			: TEXT("CapacityBlocked: native workspace working allocation peak"), OutError);
	}
	return true;
}

bool FVoxelGenerationWorkspace::SettleBytes(
	const uint64 InOldBytes, const uint64 InReservedBytes, const uint64 InActualBytes, FString& OutError)
{
	if (!Resources)
	{
		return true;
	}
	const uint64 Current = Lease ? Lease->Bytes() : 0;
	if (InActualBytes > InReservedBytes || InReservedBytes > Current || InOldBytes > Current - InReservedBytes)
	{
		return Fail(TEXT("AllocationAccountingMismatch: allocator exceeded pregranted scratch backing"), OutError);
	}
	if (Lease)
	{
		const bool bSettled = Lease->Resize(Current - InReservedBytes - InOldBytes + InActualBytes);
		check(bSettled);
	}
	return true;
}

void FVoxelGenerationWorkspace::ReleaseBytes(const uint64 InBytes)
{
	if (Lease && InBytes > 0)
	{
		const uint64 Current = Lease->Bytes();
		check(InBytes <= Current);
		const bool bSettled = Lease->Resize(Current - InBytes);
		check(bSettled);
	}
}
