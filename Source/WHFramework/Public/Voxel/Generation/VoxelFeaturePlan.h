#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"

struct WHFRAMEWORK_API FVoxelFeaturePlanWrite
{
	FIntVector Position =
		FIntVector::ZeroValue;

	uint32 Value = 0;

	EVoxelGenerationStage Stage =
		EVoxelGenerationStage::None;

	FVoxelStableId OwnerId;
};

struct WHFRAMEWORK_API FVoxelFeaturePlanKey
{
	FIntVector Position =
		FIntVector::ZeroValue;

	EVoxelGenerationStage Stage =
		EVoxelGenerationStage::None;

	bool operator==(
		const FVoxelFeaturePlanKey& InOther) const
	{
		return
			Position == InOther.Position &&
			Stage == InOther.Stage;
	}
};

FORCEINLINE uint32 GetTypeHash(
	const FVoxelFeaturePlanKey& InKey)
{
	return HashCombineFast(
		::GetTypeHash(InKey.Position),
		::GetTypeHash(
			static_cast<uint8>(
				InKey.Stage)));
}

struct WHFRAMEWORK_API FVoxelFeaturePlan
{
	FVoxelGenerationBounds Bounds;

	TArray<FVoxelFeaturePlanWrite> Writes;

	// 生成所属瓦片不等于内容范围，跨瓦片树冠、结构和洞穴必须计入实际影响范围。
	bool AffectsBounds(const FVoxelGenerationBounds& InBounds) const
	{
		return InfluenceBounds.IsValid && InfluenceBounds.Intersect(FBox(FVector(InBounds.Min), FVector(InBounds.Max)));
	}

	/** 可写 backing 必须属于当前 retained scope；失败保留旧索引，不裁合法跨 owner 内容。 */
	bool Finalize(
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);
#if WITH_DEV_AUTOMATION_TESTS
	void Finalize();
#endif

	bool Sample(
		const FIntVector& InPosition,
		EVoxelGenerationStage InStage,
		uint32& OutValue) const;

	/** 只计量化 heap backing，根对象/controller 由 Cache 单独授予。 */
	uint64 GetAllocatedBytes() const;

private:
	friend class FVoxelFeaturePlanner;
	void ReleaseBacking();

	FBox InfluenceBounds = FBox(ForceInit);
	TMap<
		FVoxelFeaturePlanKey,
		FVoxelFeaturePlanWrite>
		ResolvedWrites;
};
