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

struct WHFRAMEWORK_API FVoxelTreeRejectDiagnostics
{
	int32 Biome = 0;
	int32 Slope = 0;
	int32 Temperature = 0;
	int32 Moisture = 0;
	int32 Water = 0;
	int32 Chance = 0;
	int32 Base = 0;
};

struct WHFRAMEWORK_API FVoxelFlowerRejectDiagnostics
{
	int32 Biome = 0;
	int32 Slope = 0;
	int32 Climate = 0;
	int32 River = 0;
	int32 Surface = 0;
	int32 Chance = 0;
};

enum class EVoxelEcologyRejectReason : uint8
{
	None = 0,
	Biome,
	Slope,
	Temperature,
	Moisture,
	Water,
	Surface
};

struct WHFRAMEWORK_API FVoxelEcologySample
{
	uint16 BiomeIndex = MAX_uint16;
	uint16 TreeDensity = 0;
	uint16 GrassDensity = 0;
	uint16 FlowerDensity = 0;
	bool bTreeAllowed = false;
	bool bGrassAllowed = false;
	bool bFlowerAllowed = false;
	EVoxelEcologyRejectReason TreeRejectReason = EVoxelEcologyRejectReason::None;
	EVoxelEcologyRejectReason FlowerRejectReason = EVoxelEcologyRejectReason::None;
};

struct WHFRAMEWORK_API FVoxelEcologyPlan
{
	FVoxelGenerationBounds Bounds;
	TArray<FVoxelEcologyPlanWrite> Writes;
	int32 TreeCandidates = 0;
	int32 TreesAccepted = 0;
	FVoxelTreeRejectDiagnostics TreeRejects;
	int32 GrassPatchCandidates = 0;
	int32 GrassWrites = 0;
	int32 FlowerPatchCandidates = 0;
	int32 FlowerWrites = 0;
	FVoxelFlowerRejectDiagnostics FlowerRejects;

	// 生成所属瓦片不等于内容范围，跨瓦片树冠、结构和洞穴必须计入实际影响范围。
	bool AffectsBounds(const FVoxelGenerationBounds& InBounds) const
	{
		return InfluenceBounds.IsValid && InfluenceBounds.Intersect(FBox(FVector(InBounds.Min), FVector(InBounds.Max)));
	}

	void Finalize();
	bool Sample(const FIntVector& InPosition, uint32& OutValue) const;
	uint64 GetAllocatedBytes() const;

private:
	FBox InfluenceBounds = FBox(ForceInit);
	TMap<FIntVector, FVoxelEcologyPlanWrite> ResolvedWrites;
};

class WHFRAMEWORK_API FVoxelEcologyGenerator
{
public:
	explicit FVoxelEcologyGenerator(
		TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe);

	FVoxelEcologySample Sample(const FVoxelColumnSample& InColumn) const;

	bool BuildPlan(
		const FVoxelGenerationBounds& InBounds,
		TFunctionRef<bool(const FIntVector& InPosition, FVoxelColumnSample& OutColumn)> InSampleColumn,
		TFunctionRef<bool(const FIntVector& InPosition, uint32& OutBaseSymbol)> InSampleBaseSymbol,
		FVoxelEcologyPlan& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	void EnumerateTrees(
		const FVoxelGenerationBounds& InBounds,
		TFunctionRef<bool(const FIntVector&, FVoxelColumnSample&)> InSampleColumn,
		TFunctionRef<bool(const FIntVector&, uint32&)> InSampleBaseSymbol,
		TFunctionRef<void(const FIntVector&, int32, FVoxelStableId)> InVisit,
		int32& OutCandidates,
		int32& OutAccepted,
		const TAtomic<bool>* InCancel = nullptr,
		FVoxelTreeRejectDiagnostics* OutRejects = nullptr) const;

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

	void BuildFlowers(
		const FVoxelGenerationBounds& InBounds,
		TFunctionRef<bool(const FIntVector&, FVoxelColumnSample&)> InSampleColumn,
		TFunctionRef<bool(const FIntVector&, uint32&)> InSampleBaseSymbol,
		FVoxelEcologyPlan& InOutPlan,
		const TAtomic<bool>* InCancel) const;

	uint16 SelectFlowerSymbol(const FIntVector& InPosition,
		const FVoxelStableId& InOwnerId) const;

	static int32 EffectiveChance(int32 InChancePermille, int32 InDensityPermille);

private:
	TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
};
