#include "Voxel/Runtime/VoxelChangeIndex.h"

#include "Voxel/Generation/VoxelGenerationMath.h"

void FVoxelChangeIndex::SetModified(const FIntVector& InSection, bool bInModified)
{
	FWriteScopeLock Scope(Lock);
	const FIntVector Region = ToRegion(InSection);
	TArray<uint64>& Mask = RegionMasks.FindOrAdd(Region);
	Mask.SetNumZeroed(8);
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

void FVoxelChangeIndex::Enumerate(
	const FVoxelGenerationBounds& InBounds,
	TArray<FIntVector>& OutSections) const
{
	FReadScopeLock Guard(Lock);
	const FIntVector MinSection(
		FMath::FloorToInt(static_cast<double>(InBounds.Min.X) / 16.0),
		FMath::FloorToInt(static_cast<double>(InBounds.Min.Y) / 16.0),
		FMath::FloorToInt(static_cast<double>(InBounds.Min.Z) / 16.0));
	const FIntVector MaxSection(
		FMath::FloorToInt(static_cast<double>(InBounds.Max.X - 1) / 16.0),
		FMath::FloorToInt(static_cast<double>(InBounds.Max.Y - 1) / 16.0),
		FMath::FloorToInt(static_cast<double>(InBounds.Max.Z - 1) / 16.0));
	for (int32 Z = MinSection.Z; Z <= MaxSection.Z; ++Z)
	{
		for (int32 Y = MinSection.Y; Y <= MaxSection.Y; ++Y)
		{
			for (int32 X = MinSection.X; X <= MaxSection.X; ++X)
			{
				const FIntVector Section(X, Y, Z);
				const TArray<uint64>* Mask = RegionMasks.Find(ToRegion(Section));
				const int32 Bit = ToRegionBit(Section);
				if (Mask && Mask->IsValidIndex(Bit / 64) && (((*Mask)[Bit / 64] >> (Bit % 64)) & 1ull) != 0)
				{
					OutSections.Add(Section);
				}
			}
		}
	}
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
