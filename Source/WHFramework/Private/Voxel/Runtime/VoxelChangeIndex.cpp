#include "Voxel/Runtime/VoxelChangeIndex.h"

#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Save/VoxelRegionStore.h"

namespace
{
	uint64 MaskCount(const TConstArrayView<uint64> InMask)
	{
		uint64 Count = 0;
		for (uint64 Word : InMask)
			while (Word) { ++Count; Word &= Word - 1; }
		return Count;
	}
}

void FVoxelChangeIndex::SetBaseline(TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe> InBaseline)
{
	FWriteScopeLock Scope(Lock);
	// 初始目录尚未就绪时，不允许任何权威Final物化或编辑。
	check(RegionMasks.IsEmpty());
	Baseline = MoveTemp(InBaseline);
	ModifiedCount = Baseline ? Baseline->Records.Num() : 0;
}

void FVoxelChangeStorage::BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer)
{
	check(!Lease && Bytes == 0 && InConsumer.IsValid());
	Resources = InResources;
	Consumer = InConsumer;
}

bool FVoxelChangeStorage::Fail(const EVoxelTerrainResourceAdmission InAdmission, FString& OutError)
{
	Admission = InAdmission;
	OutError = InAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
		TEXT("Oversized: change index native storage peak") : InAdmission == EVoxelTerrainResourceAdmission::InvalidRequest ?
		TEXT("InvalidResource: change index original storage domain") : TEXT("CapacityBlocked: change index native storage");
	return false;
}

bool FVoxelChangeStorage::Prepare(const uint64 InNewBytes, FString& OutError)
{
	if (InNewBytes > MAX_uint64 - Bytes) return Fail(EVoxelTerrainResourceAdmission::CapacityUnachievable, OutError);
	if (!Resources)
	{
#if WITH_DEV_AUTOMATION_TESTS
		Bytes += InNewBytes;
		Admission = EVoxelTerrainResourceAdmission::Granted;
		return true;
#else
		return Fail(EVoxelTerrainResourceAdmission::InvalidRequest, OutError);
#endif
	}
	const uint64 Peak = Bytes + InNewBytes;
	const auto Control = Resources->GetControlConsumer();
	if (Control.IsValid() && Control.WorldEpoch != Consumer.WorldEpoch)
		return Fail(EVoxelTerrainResourceAdmission::InvalidRequest, OutError);
	FVoxelGenerationRequiredCapacity Required;
	if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, Lease.Get(), Required, OutError) ||
		RequiredInputBytes > MAX_uint64 - Required.Bytes[int32(EVoxelTerrainResourcePool::Input)] ||
		RequiredLeaseCount < 0 || RequiredLeaseCount > MAX_int32 - Required.LeaseCount)
		return Fail(EVoxelTerrainResourceAdmission::CapacityUnachievable, OutError);
	const uint64 InputFloor = RequiredInputBytes + Required.Bytes[int32(EVoxelTerrainResourcePool::Input)];
	const int32 LeaseFloor = RequiredLeaseCount + Required.LeaseCount;
	const auto Result = Lease ? Lease->TryResize(Peak, InputFloor, LeaseFloor) : Resources->TryAcquire(Consumer,
		EVoxelTerrainResourcePool::Input, Peak, Lease, 0, InputFloor, LeaseFloor);
	if (Result != EVoxelTerrainResourceAdmission::Granted) return Fail(Result, OutError);
	Bytes = Peak;
	Admission = Result;
	return true;
}

void FVoxelChangeStorage::Release()
{
	Bytes = 0;
	Lease.Reset();
	RequiredInputBytes = 0;
	RequiredLeaseCount = 0;
	Admission = EVoxelTerrainResourceAdmission::None;
}

void FVoxelChangeIndex::BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer)
{
	FWriteScopeLock Scope(Lock);
	check(RegionMasks.IsEmpty());
	Storage.BindResources(InResources, InConsumer);
}

TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> FVoxelChangeIndex::GetStorageLease() const
{
	FReadScopeLock Scope(Lock);
	return Storage.ShareLease();
}

TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> FVoxelChangeIndex::GetBaselineLease() const
{
	FReadScopeLock Scope(Lock);
	return Baseline ? Baseline->ResourceLease : nullptr;
}

bool FVoxelChangeIndex::PrepareModified(const int32 InCount, TFunctionRef<FIntVector(int32)> InSection, FString& OutError)
{
	FWriteScopeLock Scope(Lock);
	FVoxelGenerationRequiredSourceScope BaselineSource(Storage.GetResources().Get(), Baseline ? Baseline->ResourceLease.Get() : nullptr);
	if (InCount < 0) return Storage.Fail(EVoxelTerrainResourceAdmission::InvalidRequest, OutError);
	int64 NewCount = RegionMasks.Num();
	for (int32 Index = 0; Index < InCount; ++Index)
	{
		const FIntVector Region = ToRegion(InSection(Index));
		if (RegionMasks.Contains(Region)) continue;
		bool bPrevious = false;
		for (int32 Previous = 0; Previous < Index; ++Previous)
			if (ToRegion(InSection(Previous)) == Region) { bPrevious = true; break; }
		if (!bPrevious) ++NewCount;
	}
	if (NewCount > MAX_int32) return Storage.Fail(EVoxelTerrainResourceAdmission::CapacityUnachievable, OutError);
	if (!Storage.Reserve(RegionMasks, int32(NewCount), OutError)) return false;
	OutError.Reset();
	return true;
}

bool FVoxelChangeIndex::SetModified(const FIntVector& InSection, const bool bInModified, FString& OutError)
{
	FWriteScopeLock Scope(Lock);
	FVoxelGenerationRequiredSourceScope BaselineSource(Storage.GetResources().Get(), Baseline ? Baseline->ResourceLease.Get() : nullptr);
	const FIntVector Region = ToRegion(InSection);
	const auto* Base = Baseline ? Baseline->Regions.Find(Region) : nullptr;
	FRegionMask Candidate;
	if (const FRegionMask* Existing = RegionMasks.Find(Region)) Candidate = *Existing;
	else if (Base) FMemory::Memcpy(Candidate.Words, Base->ModifiedMask, sizeof(Candidate.Words));
	const uint64 Previous = MaskCount(MakeArrayView(Candidate.Words));
	const int32 Bit = ToRegionBit(InSection);
	const uint64 Flag = 1ull << (Bit & 63);
	if (bInModified) Candidate.Words[Bit >> 6] |= Flag;
	else Candidate.Words[Bit >> 6] &= ~Flag;
	const uint64 After = MaskCount(MakeArrayView(Candidate.Words));
	if (After > 0 || Base)
	{
		if (!RegionMasks.Contains(Region))
		{
			if (RegionMasks.Num() == MAX_int32) return Storage.Fail(EVoxelTerrainResourceAdmission::CapacityUnachievable, OutError);
			if (!Storage.Reserve(RegionMasks, RegionMasks.Num() + 1, OutError)) return false;
		}
		RegionMasks.FindOrAdd(Region) = Candidate;
	}
	else RegionMasks.Remove(Region);
	ModifiedCount = ModifiedCount - Previous + After;
	Storage.Succeed();
	OutError.Reset();
	return true;
}

bool FVoxelChangeIndex::IsModified(const FIntVector& InSection) const
{
	FReadScopeLock Scope(Lock);
	const FIntVector Region = ToRegion(InSection);
	const FRegionMask* Mask = RegionMasks.Find(Region);
	const auto* Base = !Mask && Baseline ? Baseline->Regions.Find(Region) : nullptr;
	const int32 Bit = ToRegionBit(InSection);
	const uint64 Word = Mask ? Mask->Words[Bit >> 6] : Base ? Base->ModifiedMask[Bit >> 6] : 0;
	return (Word & (1ull << (Bit & 63))) != 0;
}

bool FVoxelChangeIndex::SetRegionMask(const FIntVector& InRegion, const TConstArrayView<uint64> InMask, FString& OutError)
{
	FWriteScopeLock Scope(Lock);
	FVoxelGenerationRequiredSourceScope BaselineSource(Storage.GetResources().Get(), Baseline ? Baseline->ResourceLease.Get() : nullptr);
	const auto* Previous = RegionMasks.Find(InRegion);
	const auto* Base = Baseline ? Baseline->Regions.Find(InRegion) : nullptr;
	const uint64 Before = Previous ? MaskCount(MakeArrayView(Previous->Words)) : Base ? MaskCount(MakeArrayView(Base->ModifiedMask, 8)) : 0;
	FRegionMask Candidate;
	if (InMask.Num() == 8) FMemory::Memcpy(Candidate.Words, InMask.GetData(), sizeof(Candidate.Words));
	const uint64 After = MaskCount(MakeArrayView(Candidate.Words));
	if (After > 0 || Base)
	{
		if (!Previous)
		{
			if (RegionMasks.Num() == MAX_int32) return Storage.Fail(EVoxelTerrainResourceAdmission::CapacityUnachievable, OutError);
			if (!Storage.Reserve(RegionMasks, RegionMasks.Num() + 1, OutError)) return false;
		}
		RegionMasks.FindOrAdd(InRegion) = Candidate;
	}
	else RegionMasks.Remove(InRegion);
	ModifiedCount = ModifiedCount - Before + After;
	Storage.Succeed();
	OutError.Reset();
	return true;
}

void FVoxelChangeIndex::GetRegionMask(const FIntVector& InRegion, const TArrayView<uint64> OutMask) const
{
	check(OutMask.Num() == 8);
	FReadScopeLock Scope(Lock);
	if (const FRegionMask* Found = RegionMasks.Find(InRegion)) FMemory::Memcpy(OutMask.GetData(), Found->Words, sizeof(Found->Words));
	else if (const auto* Base = Baseline ? Baseline->Regions.Find(InRegion) : nullptr) FMemory::Memcpy(OutMask.GetData(), Base->ModifiedMask, 8 * sizeof(uint64));
	else FMemory::Memzero(OutMask.GetData(), 8 * sizeof(uint64));
}

void FVoxelChangeIndex::Reset()
{
	FWriteScopeLock Scope(Lock);
	RegionMasks.Empty();
	Storage.Release();
	Baseline.Reset();
	ModifiedCount = 0;
}

bool FVoxelChangeIndex::Enumerate(const FVoxelGenerationBounds& InBounds, TArray<FIntVector>& OutSections,
	FVoxelGenerationWorkspace& InWorkspace, FString& OutError, const TAtomic<bool>* InCancel) const
{
	FReadScopeLock Scope(Lock);
	FVoxelGenerationRequiredSourceScope BaselineSource(Storage.GetResources().Get(), Baseline ? Baseline->ResourceLease.Get() : nullptr);
	FVoxelGenerationRequiredSourceScope IndexSource(Storage.GetResources().Get(), Storage.GetLease());
	int64 Count = 0;
	if (!VisitLocked(InBounds, [&Count](const FIntVector&) { ++Count; }, InCancel))
	{
		OutError = TEXT("Canceled: change index enumeration");
		return false;
	}
	if (Count > MAX_int32)
	{
		OutError = TEXT("Oversized: change index enumeration");
		return false;
	}
	TArray<FIntVector> Candidate;
	if (!InWorkspace.ReserveArray(Candidate, int32(Count), OutError)) return false;
	bool bMismatch = false;
	if (!VisitLocked(InBounds, [&Candidate, &bMismatch](const FIntVector& InSection)
		{
			if (Candidate.Num() == Candidate.Max()) bMismatch = true;
			else Candidate.Add(InSection);
		}, InCancel) || bMismatch)
	{
		InWorkspace.FreeArray(Candidate);
		OutError = bMismatch ? TEXT("DependencyNotReady: change index changed during enumeration") : TEXT("Canceled: change index enumeration");
		return false;
	}
	Candidate.Sort([](const FIntVector& InA, const FIntVector& InB)
	{
		if (InA.Z != InB.Z) return InA.Z < InB.Z;
		if (InA.Y != InB.Y) return InA.Y < InB.Y;
		return InA.X < InB.X;
	});
	if (InCancel && InCancel->Load())
	{
		InWorkspace.FreeArray(Candidate);
		OutError = TEXT("Canceled: change index enumeration");
		return false;
	}
	InWorkspace.FreeArray(OutSections);
	OutSections = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
void FVoxelChangeIndex::SetModified(const FIntVector& InSection, const bool bInModified)
{
	FString Error;
	check(SetModified(InSection, bInModified, Error));
}

void FVoxelChangeIndex::SetRegionMask(const FIntVector& InRegion, const TArray<uint64>& InMask)
{
	FString Error;
	check(SetRegionMask(InRegion, MakeArrayView(InMask), Error));
}

void FVoxelChangeIndex::GetRegionMask(const FIntVector& InRegion, TArray<uint64>& OutMask) const
{
	OutMask.SetNumUninitialized(8);
	GetRegionMask(InRegion, MakeArrayView(OutMask));
}

bool FVoxelChangeIndex::Enumerate(const FVoxelGenerationBounds& InBounds,
	TArray<FIntVector>& OutSections, const TAtomic<bool>* InCancel) const
{
	FVoxelGenerationWorkspace Workspace;
	FString Error;
	return Enumerate(InBounds, OutSections, Workspace, Error, InCancel);
}
#endif

int32 FVoxelChangeIndex::Count(const FVoxelGenerationBounds& InBounds) const
{
	int32 Count = 0;
	Visit(InBounds, [&Count](const FIntVector&) { check(Count < MAX_int32); ++Count; });
	return Count;
}

bool FVoxelChangeIndex::Visit(const FVoxelGenerationBounds& InBounds,
	TFunctionRef<void(const FIntVector&)> InVisit, const TAtomic<bool>* InCancel) const
{
	FReadScopeLock Guard(Lock);
	return VisitLocked(InBounds, InVisit, InCancel);
}

bool FVoxelChangeIndex::VisitLocked(const FVoxelGenerationBounds& InBounds,
	TFunctionRef<void(const FIntVector&)> InVisit, const TAtomic<bool>* InCancel) const
{
	if (InCancel && InCancel->Load()) return false;
	if (!InBounds.IsValid()) return true;
	const FIntVector MinSection(VoxelGeneration::FloorDivide(InBounds.Min.X, 16),
		VoxelGeneration::FloorDivide(InBounds.Min.Y, 16), VoxelGeneration::FloorDivide(InBounds.Min.Z, 16));
	const FIntVector MaxSection(VoxelGeneration::FloorDivide(InBounds.Max.X - 1, 16),
		VoxelGeneration::FloorDivide(InBounds.Max.Y - 1, 16), VoxelGeneration::FloorDivide(InBounds.Max.Z - 1, 16));
	auto VisitMask = [&](const FIntVector& InRegion, const TConstArrayView<uint64> InMask)
	{
		const FInt64Vector RegionMin(int64(InRegion.X) * 8, int64(InRegion.Y) * 8, int64(InRegion.Z) * 8);
		const FInt64Vector RegionMax = RegionMin + FInt64Vector(7);
		if (RegionMin.X > MaxSection.X || RegionMin.Y > MaxSection.Y || RegionMin.Z > MaxSection.Z ||
			RegionMax.X < MinSection.X || RegionMax.Y < MinSection.Y || RegionMax.Z < MinSection.Z) return;
		for (int32 WordIndex = 0; WordIndex < InMask.Num(); ++WordIndex)
		{
			uint64 Word = InMask[WordIndex];
			while (Word != 0)
			{
				const FIntVector Section = RegionBitToSection(InRegion, WordIndex * 64 + FMath::CountTrailingZeros64(Word));
				if (Section.X >= MinSection.X && Section.X <= MaxSection.X &&
					Section.Y >= MinSection.Y && Section.Y <= MaxSection.Y && Section.Z >= MinSection.Z && Section.Z <= MaxSection.Z)
					InVisit(Section);
				Word &= Word - 1;
			}
		}
	};
	if (Baseline && !Baseline->VisitRegions(ToRegion(MinSection), ToRegion(MaxSection),
		[&](const FIntVector& InRegion, const FVoxelRegionFileHeader& InHeader)
		{
			if (!RegionMasks.Contains(InRegion)) VisitMask(InRegion, MakeArrayView(InHeader.ModifiedMask, 8));
		}, InCancel)) return false;
	for (const auto& Pair : RegionMasks)
	{
		if (InCancel && InCancel->Load()) return false;
		VisitMask(Pair.Key, MakeArrayView(Pair.Value.Words));
	}
	return !InCancel || !InCancel->Load();
}
bool FVoxelChangeIndex::HasAnyModified() const
{
	FReadScopeLock Scope(Lock);
	return ModifiedCount > 0;
}

int32 FVoxelChangeIndex::PositiveMod(int32 InValue, int32 InDivisor)
{
	int32 Result = InValue % InDivisor;
	if (Result < 0) Result += InDivisor;
	return Result;
}

FIntVector FVoxelChangeIndex::ToRegion(const FIntVector& InSection)
{
	return FIntVector(
		VoxelGeneration::FloorDivide(InSection.X, 8),
		VoxelGeneration::FloorDivide(InSection.Y, 8),
		VoxelGeneration::FloorDivide(InSection.Z, 8));
}

int32 FVoxelChangeIndex::ToRegionBit(const FIntVector& InSection)
{
	const int32 X = PositiveMod(InSection.X, 8);
	const int32 Y = PositiveMod(InSection.Y, 8);
	const int32 Z = PositiveMod(InSection.Z, 8);
	return X + Y * 8 + Z * 64;
}

FIntVector FVoxelChangeIndex::RegionBitToSection(
	const FIntVector& InRegion,
	const int32 InBit)
{
	return InRegion * 8 + FIntVector(
		InBit & 7,
		(InBit >> 3) & 7,
		(InBit >> 6) & 7);
}
