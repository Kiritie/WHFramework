#pragma once

#include "CoreMinimal.h"
#include "Voxel/Authoring/VoxelViewProfile.h"

struct WHFRAMEWORK_API FVoxelPublishGroupKey
{
	uint8 Representation = 0;
	FIntVector Coordinate = FIntVector::ZeroValue;
	uint8 Level = 0;

	FVoxelPublishGroupKey() = default;
	FVoxelPublishGroupKey(uint8 InRepresentation, FIntVector InCoordinate, uint8 InLevel)
		: Representation(InRepresentation), Coordinate(InCoordinate), Level(InLevel)
	{
	}

	FVoxelPublishGroupKey(uint8 InRepresentation, FIntPoint InCoordinate, uint8 InLevel)
		: FVoxelPublishGroupKey(InRepresentation, FIntVector(InCoordinate.X, InCoordinate.Y, 0), InLevel)
	{
	}

	bool operator==(const FVoxelPublishGroupKey& InOther) const = default;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelPublishGroupKey& InKey)
{
	return HashCombineFast(
		HashCombineFast(::GetTypeHash(InKey.Representation), GetTypeHash(InKey.Coordinate)),
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
		double InBlockSize, const FVoxelLodTopologySettings& InLod = FVoxelLodTopologySettings());

	static FBox PublicationBounds(
		const FVoxelPublishGroupKey& InKey,
		double InBlockSize,
		const FBox& InPresentedBounds,
		const FBox& InDesiredBounds,
		bool bInTransition, const FVoxelLodTopologySettings& InLod = FVoxelLodTopologySettings());

	static void Build(
		TConstArrayView<FVoxelPublishFootprint> InFootprints,
		TArray<int32>& OutGroupIndices);
};
