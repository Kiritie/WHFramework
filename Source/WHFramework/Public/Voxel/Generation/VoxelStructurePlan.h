#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"
#include "Voxel/Generation/VoxelStructure.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"

struct WHFRAMEWORK_API FVoxelStructurePlanWrite
{
	FIntVector Position =
		FIntVector::ZeroValue;

	uint32 Value = 0;

	EVoxelGenerationStage Stage =
		EVoxelGenerationStage::None;

	FVoxelStableId OwnerId;
};

struct WHFRAMEWORK_API FVoxelStructurePlanClear
{
	FVoxelGenerationBounds Bounds;

	EVoxelGenerationStage Stage =
		EVoxelGenerationStage::None;

	FVoxelStableId OwnerId;
};

struct WHFRAMEWORK_API FVoxelStructurePlanKey
{
	FIntVector Position =
		FIntVector::ZeroValue;

	EVoxelGenerationStage Stage =
		EVoxelGenerationStage::None;

	bool operator==(
		const FVoxelStructurePlanKey& InOther) const
	{
		return
			Position == InOther.Position &&
			Stage == InOther.Stage;
	}
};

FORCEINLINE uint32 GetTypeHash(
	const FVoxelStructurePlanKey& InKey)
{
	return HashCombineFast(
		::GetTypeHash(InKey.Position),
		::GetTypeHash(
			static_cast<uint8>(
				InKey.Stage)));
}

struct WHFRAMEWORK_API FVoxelStructurePlan
{
	FVoxelGenerationBounds Bounds;

	TArray<FVoxelStructurePlanClear> Clears;
	TArray<FVoxelStructurePlanWrite> Writes;
	TArray<FVoxelStructureDetailPlacement> Details;

	void Finalize();

	bool IsCleared(
		const FIntVector& InPosition,
		EVoxelGenerationStage InStage) const;

	bool Sample(
		const FIntVector& InPosition,
		EVoxelGenerationStage InStage,
		uint32& OutValue) const;

	uint64 GetAllocatedBytes() const;

private:
	TMap<
		FVoxelStructurePlanKey,
		FVoxelStructurePlanWrite>
		ResolvedWrites;
};
