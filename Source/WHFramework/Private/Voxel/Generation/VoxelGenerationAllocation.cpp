#include "Voxel/Generation/VoxelGenerationAllocation.h"

thread_local FVoxelGenerationAllocationScope* FVoxelGenerationAllocationScope::ActiveScope = nullptr;

namespace
{
	uint64 GenerationResidentLimit(const FVoxelTerrainResourceLimits& InLimits)
	{
		return InLimits.ResidentBytes - InLimits.OverlapBytes;
	}
}

FVoxelGenerationAllocationScope::FVoxelGenerationAllocationScope(
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InWorkingLease,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InResidentReservation,
	const uint64 InRootBytes)
	: Previous(ActiveScope)
	, Resources(MoveTemp(InResources))
	, Consumer(InConsumer)
	, WorkingLease(MoveTemp(InWorkingLease))
	, ResidentReservation(MoveTemp(InResidentReservation))
	, RootBytes(InRootBytes)
{
	ActiveScope = this;
	if (Resources && (!Consumer.IsValid() || !WorkingLease || !ResidentReservation || RootBytes == 0 ||
		!WorkingLease->IsOwnedBy(*Resources) || !ResidentReservation->IsOwnedBy(*Resources) ||
		WorkingLease->Bytes() != RootBytes || ResidentReservation->Bytes() != RootBytes))
	{
		FCString::Strncpy(Error, TEXT("InvalidAllocationScope: native body requires original complete reservations"), UE_ARRAY_COUNT(Error));
	}
}

FVoxelGenerationAllocationScope::~FVoxelGenerationAllocationScope()
{
	check(ActiveScope == this);
	ActiveScope = Previous;
}

FVoxelGenerationAllocationScope* FVoxelGenerationAllocationScope::Get()
{
	return ActiveScope;
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
	if (Peak > Limits.WorkingBytes || Final > GenerationResidentLimit(Limits))
	{
		return Fail(TEXT("Oversized: retained body or allocation peak exceeds original pool limit"), OutError);
	}
	if (!ResidentReservation->Resize(Final))
	{
		return Fail(TEXT("CapacityBlocked: native retained resident standby"), OutError);
	}
	if (!WorkingLease->Resize(Peak))
	{
		const bool bRestored = ResidentReservation->Resize(Current);
		check(bRestored);
		return Fail(TEXT("CapacityBlocked: native retained working allocation peak"), OutError);
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
	: Scope(FVoxelGenerationAllocationScope::Get())
{
}

FVoxelGenerationWorkspace::~FVoxelGenerationWorkspace()
{
	// 受管容器按声明顺序已经销毁；原 Working 租约在此生命周期之后退出。
}

bool FVoxelGenerationWorkspace::Check(FString& OutError) const
{
	if (Scope && Scope != FVoxelGenerationAllocationScope::Get())
	{
		return Scope->Fail(TEXT("InvalidAllocationScope: workspace used outside its original body scope"), OutError);
	}
	return Scope ? Scope->Check(OutError) : FVoxelGenerationAllocationScope::AllowPassive(OutError);
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
	if (!Scope || !Scope->Resources)
	{
		return true;
	}
	const uint64 Current = Lease ? Lease->Bytes() : 0;
	if (InOldBytes > Current)
	{
		return Fail(TEXT("AllocationAccountingMismatch: scratch backing is not covered by its workspace"), OutError);
	}
	if (InNewBytes > MAX_uint64 - Current || Current + InNewBytes > Scope->Resources->Snapshot().Limits.WorkingBytes)
	{
		return Fail(TEXT("Oversized: native workspace allocation peak exceeds original pool limit"), OutError);
	}
	const uint64 Peak = Current + InNewBytes;
	if (Peak == 0)
	{
		return true;
	}
	if (Lease ? !Lease->Resize(Peak) :
		!Scope->Resources->Acquire(Scope->Consumer, EVoxelTerrainResourcePool::Working, Peak, Lease))
	{
		return Fail(TEXT("CapacityBlocked: native workspace working allocation peak"), OutError);
	}
	return true;
}

bool FVoxelGenerationWorkspace::SettleBytes(
	const uint64 InOldBytes, const uint64 InReservedBytes, const uint64 InActualBytes, FString& OutError)
{
	if (!Scope || !Scope->Resources)
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
