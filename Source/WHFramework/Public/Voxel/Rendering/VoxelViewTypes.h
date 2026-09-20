#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"

struct WHFRAMEWORK_API FVoxelViewKey
{
	FIntVector Coordinate = FIntVector::ZeroValue;
	uint8 Level = 0;

	int32 GetStep() const;
	int32 GetSide() const;
	FVoxelGenerationBounds GetBounds() const;
	FVoxelViewKey GetParent() const;
	void GetChildren(TArray<FVoxelViewKey>& OutChildren) const;
	bool operator==(const FVoxelViewKey& InOther) const;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelViewKey& InKey)
{
	return HashCombineFast(::GetTypeHash(InKey.Coordinate), ::GetTypeHash(InKey.Level));
}

struct WHFRAMEWORK_API FVoxelSurfaceTileKey
{
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	uint8 Level = 0;

	bool operator==(const FVoxelSurfaceTileKey& InOther) const;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelSurfaceTileKey& InKey)
{
	return HashCombineFast(::GetTypeHash(InKey.Coordinate), ::GetTypeHash(InKey.Level));
}

struct WHFRAMEWORK_API FVoxelMacroTileKey
{
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	uint8 Level = 0;

	bool operator==(const FVoxelMacroTileKey& InOther) const;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelMacroTileKey& InKey)
{
	return HashCombineFast(::GetTypeHash(InKey.Coordinate), ::GetTypeHash(InKey.Level));
}
