#pragma once

#include "CoreMinimal.h"

struct WHFRAMEWORK_API FVoxelPublishGroupKey
{
	uint8 Representation = 0;
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	uint8 Level = 0;

	bool operator==(const FVoxelPublishGroupKey& InOther) const = default;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelPublishGroupKey& InKey)
{
	return HashCombineFast(
		HashCombineFast(::GetTypeHash(InKey.Representation), ::GetTypeHash(InKey.Coordinate)),
		::GetTypeHash(InKey.Level));
}

struct WHFRAMEWORK_API FVoxelPublishFootprint
{
	FVoxelPublishGroupKey Key;
	FBox Bounds = FBox(ForceInit);
	bool bTransition = false;
};

class WHFRAMEWORK_API FVoxelPublishGroupPlanner
{
public:
	static FBox OwnershipBounds(
		const FVoxelPublishGroupKey& InKey,
		double InBlockSize);

	static FBox PublicationBounds(
		const FVoxelPublishGroupKey& InKey,
		double InBlockSize,
		const FBox& InPresentedBounds,
		const FBox& InDesiredBounds,
		bool bInTransition);

	static void Build(
		TConstArrayView<FVoxelPublishFootprint> InFootprints,
		TArray<int32>& OutGroupIndices);
};
