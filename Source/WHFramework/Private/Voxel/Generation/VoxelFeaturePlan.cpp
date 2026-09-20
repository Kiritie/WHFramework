#include "Voxel/Generation/VoxelFeaturePlan.h"

void FVoxelFeaturePlan::Finalize()
{
	ResolvedWrites.Reset();

	ResolvedWrites.Reserve(
		Writes.Num());

	for (const FVoxelFeaturePlanWrite& Write :
		Writes)
	{
		const FVoxelFeaturePlanKey Key {
			Write.Position,
			Write.Stage
		};

		FVoxelFeaturePlanWrite* Existing =
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

bool FVoxelFeaturePlan::Sample(
	const FIntVector& InPosition,
	const EVoxelGenerationStage InStage,
	uint32& OutValue) const
{
	const FVoxelFeaturePlanWrite* Write =
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

uint64 FVoxelFeaturePlan::GetAllocatedBytes() const
{
	return
		static_cast<uint64>(
			Writes.GetAllocatedSize()) +
		static_cast<uint64>(
			ResolvedWrites.GetAllocatedSize());
}
