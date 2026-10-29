#pragma once

class FVoxelTerrainResourceLease;

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
	// 原正文 grant 的共享来源；深层 backing 先析构，controller deleter 仍保活到最后 weak。
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
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

	bool Reserve(int32 InWriteCount, FString& OutError);
	bool Finalize(FString* OutError = nullptr);
	bool Sample(const FIntVector& InPosition, uint32& OutValue) const;
	/** 仅持有的 native heap backing；root、共享控制器与退休任务由 Cache 构建路径授予。 */
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

	static bool ContainsCrown(const FIntVector& InOffset, int32 InRadius, const FVoxelStableId& InTreeId, EVoxelTreeForm InForm = EVoxelTreeForm::Broadleaf);
	FVoxelTreeSpecies SelectTreeSpecies(const FVoxelColumnSample& InColumn, const FVoxelStableId& InTreeId) const;
	static uint16 SelectLeaf(const FVoxelTreeSpecies& InSpecies, int32 InZ, int32 InRadius);

	bool BuildPlan(
		const FVoxelGenerationBounds& InBounds,
		TFunctionRef<bool(const FIntVector& InPosition, FVoxelColumnSample& OutColumn)> InSampleColumn,
		TFunctionRef<bool(const FIntVector& InPosition, uint32& OutBaseSymbol)> InSampleBaseSymbol,
		FVoxelEcologyPlan& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	// 树根列只在当前visitor调用内有效，后续处理直接消费本次采样。
	void EnumerateTrees(
		const FVoxelGenerationBounds& InBounds,
		TFunctionRef<bool(const FIntVector&, FVoxelColumnSample&)> InSampleColumn,
		TFunctionRef<bool(const FIntVector&, const FVoxelColumnSample&, uint32&)> InSampleBaseSymbol,
		TFunctionRef<void(const FIntVector&, int32, FVoxelStableId, const FVoxelColumnSample&)> InVisit,
		int32& OutCandidates,
		int32& OutAccepted,
		const TAtomic<bool>* InCancel = nullptr,
		FVoxelTreeRejectDiagnostics* OutRejects = nullptr) const;

private:
	static bool AppendWorldWrite(
		TFunctionRef<bool(const FVoxelEcologyPlanWrite&)> InEmit,
		const FInt64Vector& InPosition,
		uint32 InValue,
		uint8 InPriority,
		const FVoxelStableId& InOwnerId);

	void BuildTrees(
		const FVoxelGenerationBounds& InBounds,
		TFunctionRef<bool(const FIntVector&, FVoxelColumnSample&)> InSampleColumn,
		TFunctionRef<bool(const FIntVector&, uint32&)> InSampleBaseSymbol,
		FVoxelEcologyPlan& InOutPlan,
		TFunctionRef<bool(const FVoxelEcologyPlanWrite&)> InEmit,
		const TAtomic<bool>* InCancel) const;

	void BuildGrass(
		const FVoxelGenerationBounds& InBounds,
		TFunctionRef<bool(const FIntVector&, FVoxelColumnSample&)> InSampleColumn,
		TFunctionRef<bool(const FIntVector&, uint32&)> InSampleBaseSymbol,
		FVoxelEcologyPlan& InOutPlan,
		TFunctionRef<bool(const FVoxelEcologyPlanWrite&)> InEmit,
		const TAtomic<bool>* InCancel) const;

	void BuildFlowers(
		const FVoxelGenerationBounds& InBounds,
		TFunctionRef<bool(const FIntVector&, FVoxelColumnSample&)> InSampleColumn,
		TFunctionRef<bool(const FIntVector&, uint32&)> InSampleBaseSymbol,
		FVoxelEcologyPlan& InOutPlan,
		TFunctionRef<bool(const FVoxelEcologyPlanWrite&)> InEmit,
		const TAtomic<bool>* InCancel) const;

	uint16 SelectFlowerSymbol(const FIntVector& InPosition,
		const FVoxelStableId& InOwnerId) const;

	static int32 EffectiveChance(int32 InChancePermille, int32 InDensityPermille);

private:
	TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
};
