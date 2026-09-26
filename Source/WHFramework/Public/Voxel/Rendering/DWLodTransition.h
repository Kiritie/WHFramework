#pragma once

#include "CoreMinimal.h"

enum class EVoxelLodEdgeDirection : uint8
{
	None = 0,
	NegativeX,
	PositiveX,
	NegativeY,
	PositiveY
};

enum class EVoxelHeightfieldRepresentation : uint8
{
	None = 0,
	Surface,
	Macro
};

enum class EVoxelVolumeFaceDirection : uint8
{
	PositiveX = 0,
	NegativeX,
	PositiveY,
	NegativeY,
	PositiveZ,
	NegativeZ
};

struct WHFRAMEWORK_API FVoxelHeightfieldNodeKey
{
	EVoxelHeightfieldRepresentation Representation = EVoxelHeightfieldRepresentation::None;
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	uint8 Level = 0;

	bool operator==(const FVoxelHeightfieldNodeKey& InOther) const
	{
		return Representation == InOther.Representation &&
			Coordinate == InOther.Coordinate && Level == InOther.Level;
	}
};

FORCEINLINE uint32 GetTypeHash(const FVoxelHeightfieldNodeKey& InKey)
{
	return HashCombineFast(
		HashCombineFast(GetTypeHash(static_cast<uint8>(InKey.Representation)),
			GetTypeHash(InKey.Coordinate)), GetTypeHash(InKey.Level));
}

struct WHFRAMEWORK_API FVoxelHeightfieldTransitionEdge
{
	FVoxelHeightfieldNodeKey Owner;
	FVoxelHeightfieldNodeKey Neighbor;
	EVoxelLodEdgeDirection Direction = EVoxelLodEdgeDirection::None;
	int32 RangeMin = 0;
	int32 RangeMax = 0;
	int32 OwnerStep = 1;
	int32 NeighborStep = 1;
	uint64 OwnerRevision = 0;
	uint64 NeighborRevision = 0;
	bool bPending = false;
	bool bUnbalanced = false;
};
