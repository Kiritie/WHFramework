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

	void Finalize();

	bool Sample(
		const FIntVector& InPosition,
		EVoxelGenerationStage InStage,
		uint32& OutValue) const;

	uint64 GetAllocatedBytes() const;

private:
	TMap<
		FVoxelFeaturePlanKey,
		FVoxelFeaturePlanWrite>
		ResolvedWrites;
};
