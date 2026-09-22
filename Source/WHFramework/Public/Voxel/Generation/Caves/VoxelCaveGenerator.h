#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

using FVoxelCaveColumnSampler =
	TFunctionRef<
		bool(
			const FIntVector&,
			FVoxelColumnSample&)>;

struct WHFRAMEWORK_API FVoxelCaveSegment
{
	FIntVector Start = FIntVector::ZeroValue;
	FIntVector End = FIntVector::ZeroValue;
	int32 Radius = 2;

	bool IsValid() const
	{
		return Radius > 0;
	}
};

struct WHFRAMEWORK_API FVoxelCavePlan
{
	TArray<FVoxelCaveSegment> Segments;

	// 生成所属瓦片不等于内容范围，跨瓦片树冠、结构和洞穴必须计入实际影响范围。
	bool AffectsBounds(const FVoxelGenerationBounds& InBounds) const
	{
		return InfluenceBounds.IsValid && InfluenceBounds.Intersect(FBox(FVector(InBounds.Min), FVector(InBounds.Max)));
	}

	void Finalize();
	bool Carves(const FIntVector& InCell) const;
	bool ProtectsFloor(const FIntVector& InCell) const;
	uint64 GetAllocatedBytes() const;

private:
	FBox InfluenceBounds = FBox(ForceInit);
	TMap<FIntVector, TArray<int32>>
		SegmentIndicesBySection;
};

class WHFRAMEWORK_API FVoxelCaveGenerator
{
public:
	explicit FVoxelCaveGenerator(
		TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe);

	bool BuildPlan(
		const FVoxelGenerationBounds& InBounds,
		FVoxelCaveColumnSampler InColumnSampler,
		FVoxelCavePlan& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	static int32 GetMaximumReach(const FVoxelGenerationSettings& InSettings)
	{
		return MaximumSegments * MaximumSegmentLength + MaximumRoomRadius +
			FMath::Max(InSettings.CaveMainRadius, InSettings.CaveBranchRadius) +
			InSettings.CaveEntranceLength + 8;
	}

private:
	bool TryBuildSystem(
		const FIntPoint& InAnchorGrid,
		const FVoxelGenerationBounds& InOwnerBounds,
		FVoxelCaveColumnSampler InColumnSampler,
		TArray<FVoxelCaveSegment>& OutSegments,
		FString& OutError,
		const TAtomic<bool>* InCancel) const;

	bool AddBranch(
		FRandomStream& InStream,
		const FIntVector& InStart,
		double InYaw,
		double InPitch,
		FVoxelCaveColumnSampler InColumnSampler,
		TArray<FVoxelCaveSegment>& OutSegments,
		const TAtomic<bool>* InCancel) const;

	bool BuildEntranceCorridor(
		FRandomStream& InStream,
		const FIntPoint& InStartXY,
		const FVoxelColumnSample& InStartColumn,
		FVoxelCaveColumnSampler InColumnSampler,
		TArray<FVoxelCaveSegment>& InOutSegments,
		FIntVector& OutEnd,
		double& OutYaw,
		FString& OutError,
		const TAtomic<bool>* InCancel) const;

	FIntVector ClampBelowSurface(
		const FIntVector& InPosition,
		FVoxelCaveColumnSampler InColumnSampler) const;

	bool IsDeepEnoughForInterior(
		const FIntVector& InPosition,
		FVoxelCaveColumnSampler InColumnSampler) const;

private:
	static constexpr int32 MinimumSegments = 14;
	static constexpr int32 MaximumSegments = 28;
	static constexpr int32 MinimumSegmentLength = 3;
	static constexpr int32 MaximumSegmentLength = 6;
	static constexpr int32 MaximumRoomRadius = 4;

	TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
};
