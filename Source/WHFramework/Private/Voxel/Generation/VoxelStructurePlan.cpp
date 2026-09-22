#include "Voxel/Generation/VoxelStructurePlan.h"

void FVoxelStructurePlan::Finalize()
{
	InfluenceBounds.Init();
	for (const FVoxelStructurePlanClear& Clear : Clears)
	{
		InfluenceBounds += FBox(FVector(Clear.Bounds.Min), FVector(Clear.Bounds.Max));
	}

	ResolvedWrites.Reset();

	ResolvedWrites.Reserve(
		Writes.Num());

	for (const FVoxelStructurePlanWrite& Write :
		Writes)
	{
		InfluenceBounds += FVector(Write.Position);
		InfluenceBounds += FVector(Write.Position) + FVector(1.0);
		const FVoxelStructurePlanKey Key {
			Write.Position,
			Write.Stage
		};

		FVoxelStructurePlanWrite* Existing =
			ResolvedWrites.Find(Key);

		if (!Existing ||
			Existing->OwnerId <
				Write.OwnerId)
		{
			ResolvedWrites.Add(
				Key,
				Write);
		}
	}
}

bool FVoxelStructurePlan::IsCleared(
	const FIntVector& InPosition,
	const EVoxelGenerationStage InStage) const
{
	for (const FVoxelStructurePlanClear& Clear :
		Clears)
	{
		if (Clear.Stage ==
				InStage &&
			Clear.Bounds.Contains(
				InPosition))
		{
			return true;
		}
	}

	return false;
}

bool FVoxelStructurePlan::Sample(
	const FIntVector& InPosition,
	const EVoxelGenerationStage InStage,
	uint32& OutValue) const
{
	const FVoxelStructurePlanWrite* Write =
		ResolvedWrites.Find({
			InPosition,
			InStage
		});

	if (!Write)
	{
		return false;
	}

	OutValue =
		Write->Value;

	return true;
}

uint64 FVoxelStructurePlan::GetAllocatedBytes() const
{
	return
		sizeof(FVoxelStructurePlan) +
		Clears.GetAllocatedSize() +
		Writes.GetAllocatedSize() +
		Details.GetAllocatedSize() +
		ResolvedWrites.GetAllocatedSize();
}
