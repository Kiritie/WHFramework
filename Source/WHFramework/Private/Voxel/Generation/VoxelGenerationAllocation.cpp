#include "Voxel/Generation/VoxelGenerationAllocation.h"

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
	uint64 GenerationResidentLimit(const FVoxelTerrainResourceLimits& InLimits)
	{
		return InLimits.ResidentBytes - InLimits.OverlapBytes;
	}
}

FVoxelGenerationRequiredSourceScope::FVoxelGenerationRequiredSourceScope(
	const FVoxelTerrainResources* InResources,
	const void* InContext,
	FEnumerate InEnumerate)
	: Previous(ActiveScopeSlot())
	, Resources(InContext && InEnumerate ? InResources : nullptr)
	, Context(InContext)
	, Enumerator(InEnumerate)
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
		InVisit({nullptr, InLease->Serial(), InLease->GetPool(), InLease->Bytes()});
	}
}

void FVoxelGenerationRequiredSourceScope::Enumerate(const FVoxelTerrainResources& InResources, FVisit InVisit)
{
	for (auto* Scope = ActiveScopeSlot(); Scope; Scope = Scope->Previous)
	{
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
	FString& OutError)
{
	return MeasureSources([&InResources](FVisit InVisit) { Enumerate(InResources, InVisit); },
		InExcludedLease ? InExcludedLease->Serial() : 0, OutCapacity, OutError);
}

bool FVoxelGenerationRequiredSourceScope::Measure(const void* InContext, FEnumerate InEnumerate,
	FVoxelGenerationRequiredCapacity& OutCapacity, FString& OutError)
{
	return MeasureSources([InContext, InEnumerate](FVisit InVisit) { InEnumerate(InContext, InVisit); },
		0, OutCapacity, OutError);
}

bool FVoxelGenerationRequiredSourceScope::MeasureSources(TFunctionRef<void(FVisit)> InEnumerate,
	const uint64 InExcludedSerial, FVoxelGenerationRequiredCapacity& OutCapacity, FString& OutError)
{
	FVoxelGenerationRequiredCapacity Capacity;
	uint64 Index = 0;
	bool bValid = true;
	InEnumerate([&](const FVoxelGenerationRequiredSource& InSource)
	{
		const uint64 SourceIndex = Index++;
		if (!bValid || (InSource.Serial && InSource.Serial == InExcludedSerial)) return;
		if ((!InSource.Serial && !InSource.Object) || InSource.Pool >= EVoxelTerrainResourcePool::Count || InSource.LeaseCount < 0)
		{
			bValid = false;
			return;
		}
		bool bDuplicate = false;
		uint64 PrefixIndex = 0;
		// 枚举前缀而不建立 heap set；同一事实由多个 reader/scope 强持仍只计一次。
		InEnumerate([&](const FVoxelGenerationRequiredSource& InPrevious)
		{
			if (PrefixIndex++ < SourceIndex && (InSource.Serial
				? InPrevious.Serial == InSource.Serial
				: !InPrevious.Serial && InPrevious.Object == InSource.Object)) bDuplicate = true;
		});
		if (bDuplicate) return;
		uint64& Bytes = Capacity.Bytes[static_cast<int32>(InSource.Pool)];
		if (InSource.Bytes > MAX_uint64 - Bytes || InSource.LeaseCount > MAX_int32 - Capacity.LeaseCount)
		{
			bValid = false;
			return;
		}
		Bytes += InSource.Bytes;
		Capacity.LeaseCount += InSource.LeaseCount;
	});
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
	const uint64 InRootBytes)
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
		WorkingLease->Bytes() != RootBytes || ResidentReservation->Bytes() != RootBytes))
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
		return Fail(TEXT("Oversized: retained body or allocation peak exceeds original pool limit"), OutError);
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
