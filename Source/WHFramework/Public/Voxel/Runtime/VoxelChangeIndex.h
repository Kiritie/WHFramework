#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"

class WHFRAMEWORK_API FVoxelChangeIndex
{
public:
	void SetModified(const FIntVector& InSection, bool bInModified);
	bool IsModified(const FIntVector& InSection) const;
	void SetRegionMask(const FIntVector& InRegion, const TArray<uint64>& InMask);
	void GetRegionMask(const FIntVector& InRegion, TArray<uint64>& OutMask) const;
	bool Enumerate(
		const FVoxelGenerationBounds& InBounds,
		TArray<FIntVector>& OutSections,
		const TAtomic<bool>* InCancel = nullptr) const;
	bool HasAnyModified() const;
	void Reset();

private:
	static int32 PositiveMod(int32 InValue, int32 InDivisor);
	static FIntVector ToRegion(const FIntVector& InSection);
	static int32 ToRegionBit(const FIntVector& InSection);
	static FIntVector RegionBitToSection(const FIntVector& InRegion, int32 InBit);

private:
	mutable FRWLock Lock;
	TMap<FIntVector, TArray<uint64>> RegionMasks;
};
