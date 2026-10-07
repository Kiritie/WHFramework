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

void FVoxelChangeIndex::SetModified(const FIntVector& InSection, bool bInModified)
{
	FWriteScopeLock Scope(Lock);
	const FIntVector Region = ToRegion(InSection);
	const auto* Base = Baseline ? Baseline->Regions.Find(Region) : nullptr;
	const bool bNewMask = !RegionMasks.Contains(Region);
	TArray<uint64>& Mask = RegionMasks.FindOrAdd(Region);
	if (Mask.Num() != 8)
	{
		if (bNewMask && Base) Mask.Append(Base->ModifiedMask, 8);
		else Mask.Init(0, 8);
	}
	const uint64 Previous = MaskCount(Mask);
	const int32 Bit = ToRegionBit(InSection);
	const uint64 Flag = 1ull << (Bit & 63);
	if (bInModified) Mask[Bit >> 6] |= Flag;
	else Mask[Bit >> 6] &= ~Flag;
	bool bAny = false;
	for (const uint64 Word : Mask) bAny |= Word != 0;
	ModifiedCount = ModifiedCount - Previous + MaskCount(Mask);
	if (!bAny && !Base) RegionMasks.Remove(Region);
}

bool FVoxelChangeIndex::IsModified(const FIntVector& InSection) const
{
	FReadScopeLock Scope(Lock);
	const FIntVector Region = ToRegion(InSection);
	const TArray<uint64>* Mask = RegionMasks.Find(Region);
	const auto* Base = !Mask && Baseline ? Baseline->Regions.Find(Region) : nullptr;
	const int32 Bit = ToRegionBit(InSection);
	const uint64 Word = Mask && Mask->Num() == 8 ? (*Mask)[Bit >> 6] : Base ? Base->ModifiedMask[Bit >> 6] : 0;
	return (Word & (1ull << (Bit & 63))) != 0;
}

void FVoxelChangeIndex::SetRegionMask(const FIntVector& InRegion, const TArray<uint64>& InMask)
{
	FWriteScopeLock Scope(Lock);
	const auto* Previous = RegionMasks.Find(InRegion);
	const auto* Base = Baseline ? Baseline->Regions.Find(InRegion) : nullptr;
	const uint64 Before = Previous ? MaskCount(*Previous) : Base ? MaskCount(MakeArrayView(Base->ModifiedMask, 8)) : 0;
	TArray<uint64> Mask;
	if (InMask.Num() == 8) Mask = InMask;
	else Mask.Init(0, 8);
	const uint64 After = MaskCount(Mask);
	ModifiedCount = ModifiedCount - Before + After;
	if (After > 0 || Base) RegionMasks.Add(InRegion, MoveTemp(Mask));
	else RegionMasks.Remove(InRegion);
}

void FVoxelChangeIndex::GetRegionMask(const FIntVector& InRegion, TArray<uint64>& OutMask) const
{
	FReadScopeLock Scope(Lock);
	if (const TArray<uint64>* Found = RegionMasks.Find(InRegion)) OutMask = *Found;
	else if (const auto* Base = Baseline ? Baseline->Regions.Find(InRegion) : nullptr)
	{
		OutMask.Reset();
		OutMask.Append(Base->ModifiedMask, 8);
	}
	else OutMask.Init(0, 8);
}

void FVoxelChangeIndex::Reset()
{
	FWriteScopeLock Scope(Lock);
	RegionMasks.Reset();
	Baseline.Reset();
	ModifiedCount = 0;
}

bool FVoxelChangeIndex::Enumerate(const FVoxelGenerationBounds& InBounds,
	TArray<FIntVector>& OutSections, const TAtomic<bool>* InCancel) const
{
	OutSections.Reset();
	if (!Visit(InBounds, [&OutSections](const FIntVector& InSection) { OutSections.Add(InSection); }, InCancel))
	{
		OutSections.Reset();
		return false;
	}
	OutSections.Sort([](const FIntVector& InA, const FIntVector& InB)
	{
		if (InA.Z != InB.Z) return InA.Z < InB.Z;
		if (InA.Y != InB.Y) return InA.Y < InB.Y;
		return InA.X < InB.X;
	});
	return true;
}

int32 FVoxelChangeIndex::Count(const FVoxelGenerationBounds& InBounds) const
{
	int32 Count = 0;
	Visit(InBounds, [&Count](const FIntVector&) { check(Count < MAX_int32); ++Count; });
	return Count;
}

bool FVoxelChangeIndex::Visit(const FVoxelGenerationBounds& InBounds,
	TFunctionRef<void(const FIntVector&)> InVisit, const TAtomic<bool>* InCancel) const
{
	if (!InBounds.IsValid()) return true;
	FReadScopeLock Guard(Lock);
	const FIntVector MinSection(VoxelGeneration::FloorDivide(InBounds.Min.X, 16),
		VoxelGeneration::FloorDivide(InBounds.Min.Y, 16), VoxelGeneration::FloorDivide(InBounds.Min.Z, 16));
	const FIntVector MaxSection(VoxelGeneration::FloorDivide(InBounds.Max.X - 1, 16),
		VoxelGeneration::FloorDivide(InBounds.Max.Y - 1, 16), VoxelGeneration::FloorDivide(InBounds.Max.Z - 1, 16));
	auto VisitMask = [&](const FIntVector& InRegion, const TConstArrayView<uint64> InMask)
	{
		const FIntVector RegionMin = InRegion * 8;
		const FIntVector RegionMax = RegionMin + FIntVector(7);
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
		VisitMask(Pair.Key, Pair.Value);
	}
	return true;
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
