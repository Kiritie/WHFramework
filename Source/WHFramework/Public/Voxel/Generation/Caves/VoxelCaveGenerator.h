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

struct WHFRAMEWORK_API FVoxelCaveEntrance
{
	FVoxelStableId Id;
	FIntVector MouthCenter = FIntVector::ZeroValue;
	FVector2D Facing = FVector2D(1.0, 0.0);
	int32 HalfWidth = 0;
	int32 Height = 0;
	int32 Clearance = 0;
	FIntVector InteriorJoin = FIntVector::ZeroValue;
};

struct WHFRAMEWORK_API FVoxelCavePlan
{
	TArray<FVoxelCaveSegment> Segments;
	TArray<FVoxelCaveEntrance> Entrances;
	TArray<FVoxelGenerationBounds> ClearVolumes;

	// 生成所属瓦片不等于内容范围，跨瓦片树冠、结构和洞穴必须计入实际影响范围。
	bool AffectsBounds(const FVoxelGenerationBounds& InBounds) const
	{
		return InfluenceBounds.IsValid && InfluenceBounds.Intersect(FBox(FVector(InBounds.Min), FVector(InBounds.Max)));
	}

	/** active 模式的可写 backing 必须属于当前 retained scope；完整跨瓦片内容不裁 owner bounds。 */
	bool Finalize(
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);
#if WITH_DEV_AUTOMATION_TESTS
	void Finalize();
#endif
	bool Carves(const FIntVector& InCell) const;
	bool ProtectsFloor(const FIntVector& InCell) const;
	/** 包含所有嵌套 section backing，不重复计根对象/controller。 */
	uint64 GetAllocatedBytes() const;

private:
	friend class FVoxelCaveGenerator;
	void ReleaseBacking();

	FBox InfluenceBounds = FBox(ForceInit);
	TMap<FIntVector, TArray<int32>>
		SegmentIndicesBySection;
	TMap<FIntVector, TArray<int32>>
		ClearVolumeIndicesBySection;
};

class WHFRAMEWORK_API FVoxelCaveGenerator
{
public:
	explicit FVoxelCaveGenerator(
		TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe);

	/** active 输出只能是新正文或当前 scope 的 retained 正文；失败保持旧 OutPlan。 */
	bool BuildPlan(
		const FVoxelGenerationBounds& InBounds,
		FVoxelCaveColumnSampler InColumnSampler,
		FVoxelCavePlan& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	static int32 GetMaximumReach(const FVoxelGenerationSettings& InSettings)
	{
		return int32(FMath::Min<int64>(MAX_int32, int64(MaximumSegments) * MaximumSegmentLength + MaximumRoomRadius +
			FMath::Max(InSettings.CaveMainRadius, InSettings.CaveBranchRadius) +
			int64(InSettings.CaveEntranceLength) + InSettings.CaveEntranceClearance + 8));
	}

private:
	bool TryBuildSystem(
		const FIntPoint& InAnchorGrid,
		const FVoxelGenerationBounds& InOwnerBounds,
		FVoxelCaveColumnSampler InColumnSampler,
		TArray<FVoxelCaveSegment>& OutSegments,
		TArray<FVoxelCaveEntrance>& OutEntrances,
		TArray<FVoxelGenerationBounds>& OutClearVolumes,
		FString& OutError,
		const TAtomic<bool>* InCancel) const;

	bool AddBranch(
		FRandomStream& InStream,
		const FIntVector& InStart,
		double InYaw,
		double InPitch,
		FVoxelCaveColumnSampler InColumnSampler,
		TArray<FVoxelCaveSegment>& OutSegments,
		FString& OutError,
		const TAtomic<bool>* InCancel) const;

	bool BuildEntranceCorridor(
		FRandomStream& InStream,
		const FIntPoint& InStartXY,
		const FVoxelColumnSample& InStartColumn,
		FVoxelCaveColumnSampler InColumnSampler,
		TArray<FVoxelCaveSegment>& InOutSegments,
		TArray<FVoxelGenerationBounds>& InOutClearVolumes,
		FVoxelCaveEntrance& OutEntrance,
		FIntVector& OutEnd,
		double& OutYaw,
		FString& OutError,
		const TAtomic<bool>* InCancel) const;

	bool ClampBelowSurface(
		const FIntVector& InPosition,
		FVoxelCaveColumnSampler InColumnSampler,
		FIntVector& OutPosition) const;

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
