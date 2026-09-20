#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"
#include "Voxel/Generation/VoxelNaturalGenerationCache.h"

class FVoxelRiverGenerator;
class FVoxelTerrainGenerator;

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

	bool BuildPlan(
		const FVoxelLakeAnchorKey& InKey,
		FVoxelLakeAnchorPlan& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool ApplyFeature(
		const FVoxelLakeAnchorPlan& InPlan,
		int32 InX,
		int32 InY,
		FVoxelColumnSample& InOutColumn) const;

	static constexpr int32 AnchorSide = 64;

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
