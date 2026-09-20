#pragma once

#include "CoreMinimal.h"

#include "Voxel/Generation/VoxelGenerationRecipe.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"

struct WHFRAMEWORK_API FVoxelEcologyPlanWrite
{
	FIntVector Position = FIntVector::ZeroValue;
	uint32 Value = 0;
	uint8 Priority = 0;
	FVoxelStableId OwnerId;
};

struct WHFRAMEWORK_API FVoxelEcologyPlan
{
	FVoxelGenerationBounds Bounds;
	TArray<FVoxelEcologyPlanWrite> Writes;
	int32 TreeCandidates = 0;
	int32 TreesAccepted = 0;
	int32 GrassPatchCandidates = 0;
	int32 GrassWrites = 0;

	void Finalize();
	bool Sample(const FIntVector& InPosition, uint32& OutValue) const;
	uint64 GetAllocatedBytes() const;

private:
	TMap<FIntVector, FVoxelEcologyPlanWrite> ResolvedWrites;
};

class WHFRAMEWORK_API FVoxelEcologyGenerator
{
public:
	explicit FVoxelEcologyGenerator(
		TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe);

	bool BuildPlan(
		const FVoxelGenerationBounds& InBounds,
		TFunctionRef<bool(const FIntVector& InPosition, FVoxelColumnSample& OutColumn)> InSampleColumn,
		TFunctionRef<bool(const FIntVector& InPosition, uint32& OutBaseSymbol)> InSampleBaseSymbol,
		FVoxelEcologyPlan& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

private:
	void BuildTrees(
		const FVoxelGenerationBounds& InBounds,
		TFunctionRef<bool(const FIntVector&, FVoxelColumnSample&)> InSampleColumn,
		TFunctionRef<bool(const FIntVector&, uint32&)> InSampleBaseSymbol,
		FVoxelEcologyPlan& InOutPlan,
		const TAtomic<bool>* InCancel) const;

	void BuildGrass(
		const FVoxelGenerationBounds& InBounds,
		TFunctionRef<bool(const FIntVector&, FVoxelColumnSample&)> InSampleColumn,
		TFunctionRef<bool(const FIntVector&, uint32&)> InSampleBaseSymbol,
		FVoxelEcologyPlan& InOutPlan,
		const TAtomic<bool>* InCancel) const;

	bool IsTreeColumnAllowed(const FVoxelColumnSample& InColumn) const;
	bool IsGrassColumnAllowed(const FVoxelColumnSample& InColumn) const;
	static int32 EffectiveChance(int32 InChancePermille, int32 InDensityPermille);

private:
	TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
};
