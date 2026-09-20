#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"

struct WHFRAMEWORK_API FVoxelNaturalTileKey
{
	FIntPoint Coordinate = FIntPoint::ZeroValue;

	bool operator==(const FVoxelNaturalTileKey& InOther) const
	{
		return Coordinate == InOther.Coordinate;
	}
};

FORCEINLINE uint32 GetTypeHash(const FVoxelNaturalTileKey& InKey)
{
	return ::GetTypeHash(InKey.Coordinate);
}

struct WHFRAMEWORK_API FVoxelLakeAnchorKey
{
	FIntPoint Coordinate = FIntPoint::ZeroValue;

	bool operator==(const FVoxelLakeAnchorKey& InOther) const
	{
		return Coordinate == InOther.Coordinate;
	}
};

FORCEINLINE uint32 GetTypeHash(const FVoxelLakeAnchorKey& InKey)
{
	return ::GetTypeHash(InKey.Coordinate);
}

struct WHFRAMEWORK_API FVoxelBaseColumnEntry
{
	FIntPoint Position = FIntPoint::ZeroValue;
	FVoxelColumnSample Column;

	uint64 GetAllocatedBytes() const
	{
		return sizeof(FVoxelBaseColumnEntry);
	}
};

struct WHFRAMEWORK_API FVoxelRiverFieldSample
{
	int32 Distance = MAX_int32;
	int32 NormalizedDistanceQ16 = MAX_int32;
	int16 DirectionXQ15 = 32767;
	int16 DirectionYQ15 = 0;
};

struct WHFRAMEWORK_API FVoxelRiverFieldTile
{
	static constexpr int32 Side = 32;

	FVoxelNaturalTileKey Key;
	TArray<FVoxelRiverFieldSample> Samples;

	FIntPoint GetOrigin() const
	{
		return Key.Coordinate * Side;
	}

	bool Sample(
		const int32 InX,
		const int32 InY,
		FVoxelRiverFieldSample& OutSample) const
	{
		const FIntPoint Origin = GetOrigin();
		const int32 LocalX = InX - Origin.X;
		const int32 LocalY = InY - Origin.Y;
		if (LocalX < 0 || LocalY < 0 ||
			LocalX >= Side || LocalY >= Side ||
			Samples.Num() != Side * Side)
		{
			return false;
		}

		OutSample = Samples[LocalX + LocalY * Side];
		return true;
	}

	uint64 GetAllocatedBytes() const
	{
		return Samples.GetAllocatedSize();
	}
};

struct WHFRAMEWORK_API FVoxelLakeFeature
{
	FIntPoint Center = FIntPoint::ZeroValue;
	int32 WaterZ = MIN_int32;
	int32 MajorRadius = 0;
	int32 MinorRadius = 0;
	int32 Depth = 0;
	double Rotation = 0.0;

	bool IsValid() const
	{
		return WaterZ != MIN_int32 && MajorRadius > 0 && MinorRadius > 0 && Depth > 0;
	}
};

struct WHFRAMEWORK_API FVoxelLakeAnchorPlan
{
	FVoxelLakeAnchorKey Key;
	bool bHasFeature = false;
	FVoxelLakeFeature Feature;

	uint64 GetAllocatedBytes() const
	{
		return sizeof(FVoxelLakeAnchorPlan);
	}
};

struct WHFRAMEWORK_API FVoxelNaturalColumnEntry
{
	FIntPoint Position = FIntPoint::ZeroValue;
	FVoxelColumnSample Column;

	uint64 GetAllocatedBytes() const
	{
		return sizeof(FVoxelNaturalColumnEntry);
	}
};

FORCEINLINE FVoxelNaturalTileKey VoxelNaturalTileKeyFromCell(
	const int32 InX,
	const int32 InY)
{
	return {
		FIntPoint(
			VoxelGeneration::FloorDivide(InX, FVoxelRiverFieldTile::Side),
			VoxelGeneration::FloorDivide(InY, FVoxelRiverFieldTile::Side))
	};
}
