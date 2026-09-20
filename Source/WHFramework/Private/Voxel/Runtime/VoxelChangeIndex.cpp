#include "Voxel/Runtime/VoxelChangeIndex.h"

#include "Voxel/Generation/VoxelGenerationMath.h"

void FVoxelChangeIndex::SetModified(const FIntVector& InSection, bool bInModified)
{
	FWriteScopeLock Scope(Lock);
	const FIntVector Region = ToRegion(InSection);
	TArray<uint64>& Mask = RegionMasks.FindOrAdd(Region);
	if (Mask.Num() != 8)
	{
		Mask.Init(0, 8);
	}
	const int32 Bit = ToRegionBit(InSection);
	const uint64 Flag = 1ull << (Bit & 63);
	if (bInModified) Mask[Bit >> 6] |= Flag;
	else Mask[Bit >> 6] &= ~Flag;
	bool bAny = false;
	for (const uint64 Word : Mask) bAny |= Word != 0;
	if (!bAny) RegionMasks.Remove(Region);
}

bool FVoxelChangeIndex::IsModified(const FIntVector& InSection) const
{
	FReadScopeLock Scope(Lock);
	const TArray<uint64>* Mask = RegionMasks.Find(ToRegion(InSection));
	if (!Mask || Mask->Num() != 8) return false;
	const int32 Bit = ToRegionBit(InSection);
	return ((*Mask)[Bit >> 6] & (1ull << (Bit & 63))) != 0;
}

void FVoxelChangeIndex::SetRegionMask(const FIntVector& InRegion, const TArray<uint64>& InMask)
{
	FWriteScopeLock Scope(Lock);
	if (InMask.Num() != 8)
	{
		RegionMasks.Remove(InRegion);
		return;
	}
	bool bAny = false;
	for (const uint64 Word : InMask) bAny |= Word != 0;
	if (bAny) RegionMasks.Add(InRegion, InMask);
	else RegionMasks.Remove(InRegion);
}

void FVoxelChangeIndex::GetRegionMask(const FIntVector& InRegion, TArray<uint64>& OutMask) const
{
	FReadScopeLock Scope(Lock);
	if (const TArray<uint64>* Found = RegionMasks.Find(InRegion)) OutMask = *Found;
	else OutMask.Init(0, 8);
}

void FVoxelChangeIndex::Reset()
{
	FWriteScopeLock Scope(Lock);
	RegionMasks.Reset();
}

bool FVoxelChangeIndex::Enumerate(
	const FVoxelGenerationBounds& InBounds,
	TArray<FIntVector>& OutSections,
	const TAtomic<bool>* InCancel) const
{
	OutSections.Reset();
	if (InBounds.Max.X <= InBounds.Min.X ||
		InBounds.Max.Y <= InBounds.Min.Y ||
		InBounds.Max.Z <= InBounds.Min.Z)
	{
		return true;
	}

	FReadScopeLock Guard(Lock);
	const FIntVector MinSection(
		VoxelGeneration::FloorDivide(InBounds.Min.X, 16),
		VoxelGeneration::FloorDivide(InBounds.Min.Y, 16),
		VoxelGeneration::FloorDivide(InBounds.Min.Z, 16));
	const FIntVector MaxSection(
		VoxelGeneration::FloorDivide(InBounds.Max.X - 1, 16),
		VoxelGeneration::FloorDivide(InBounds.Max.Y - 1, 16),
		VoxelGeneration::FloorDivide(InBounds.Max.Z - 1, 16));

	for (const TPair<FIntVector, TArray<uint64>>& Pair : RegionMasks)
	{
		if (InCancel && InCancel->Load())
		{
			OutSections.Reset();
			return false;
		}

		const TArray<uint64>& Mask = Pair.Value;
		for (int32 WordIndex = 0; WordIndex < Mask.Num(); ++WordIndex)
		{
			uint64 Word = Mask[WordIndex];
			while (Word != 0)
			{
				const int32 LowestBit = FMath::CountTrailingZeros64(Word);
				const FIntVector Section = RegionBitToSection(
					Pair.Key,
					WordIndex * 64 + LowestBit);
				if (Section.X >= MinSection.X && Section.X <= MaxSection.X &&
					Section.Y >= MinSection.Y && Section.Y <= MaxSection.Y &&
					Section.Z >= MinSection.Z && Section.Z <= MaxSection.Z)
				{
					OutSections.Add(Section);
				}
				Word &= Word - 1;
			}
		}
	}

	OutSections.Sort([](const FIntVector& InA, const FIntVector& InB)
	{
		if (InA.Z != InB.Z) return InA.Z < InB.Z;
		if (InA.Y != InB.Y) return InA.Y < InB.Y;
		return InA.X < InB.X;
	});
	return true;
}

bool FVoxelChangeIndex::HasAnyModified() const
{
	FReadScopeLock Scope(Lock);
	return !RegionMasks.IsEmpty();
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
