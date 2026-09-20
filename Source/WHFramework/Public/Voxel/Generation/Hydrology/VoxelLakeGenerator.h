#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

class FVoxelRiverGenerator;
class FVoxelTerrainGenerator;

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
		return
			WaterZ != MIN_int32 &&
			MajorRadius > 0 &&
			MinorRadius > 0 &&
			Depth > 0;
	}
};

class WHFRAMEWORK_API FVoxelLakeGenerator
{
public:
	FVoxelLakeGenerator(
		TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe,
		TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> InTerrain,
		TSharedRef<const FVoxelRiverGenerator, ESPMode::ThreadSafe> InRiver);

	bool ApplyToColumn(
		int32 InX,
		int32 InY,
		FVoxelColumnSample& InOutColumn) const;

private:
	bool TryGetFeature(
		const FIntPoint& InAnchor,
		FVoxelLakeFeature& OutFeature) const;

	bool BuildFeature(
		const FIntPoint& InAnchor,
		FVoxelLakeFeature& OutFeature) const;

	bool EvaluateFeature(
		const FVoxelLakeFeature& InFeature,
		int32 InX,
		int32 InY,
		int32 InOriginalHeight,
		int32& OutBedZ,
		int32& OutWaterZ) const;

	double CalculateShapeAlpha(
		const FVoxelLakeFeature& InFeature,
		int32 InX,
		int32 InY) const;

	bool ValidateBoundary(
		const FVoxelLakeFeature& InFeature) const;

private:
	static constexpr int32 AnchorSide = 64;
	static constexpr int32 SpawnPermille = 350;
	static constexpr int32 MinimumRadius = 4;
	static constexpr int32 MaximumRadius = 30;
	static constexpr int32 MinimumDepth = 3;
	static constexpr int32 MaximumDepth = 5;

	TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
	TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> Terrain;
	TSharedRef<const FVoxelRiverGenerator, ESPMode::ThreadSafe> River;

	mutable TMap<FIntPoint, TOptional<FVoxelLakeFeature>>
		FeatureCache;
};
