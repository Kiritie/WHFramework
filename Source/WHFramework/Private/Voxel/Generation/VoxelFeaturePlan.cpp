#include "Voxel/Generation/VoxelFeaturePlan.h"

#include "Misc/ScopeExit.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

bool FVoxelFeaturePlan::Finalize(FString& OutError, const TAtomic<bool>* InCancel)
{
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	TMap<FVoxelFeaturePlanKey, FVoxelFeaturePlanWrite> NewResolved;
	ON_SCOPE_EXIT
	{
		VoxelGenerationAllocation::FreeRetainedMap(NewResolved);
	};
	if (!VoxelGenerationAllocation::RetainMap(NewResolved, Writes.Num(), OutError))
	{
		return false;
	}
	FBox NewInfluence(ForceInit);

	for (const FVoxelFeaturePlanWrite& Write :
		Writes)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		if (!VoxelGeneration::IsWorldCell(Write.Position.X) ||
			!VoxelGeneration::IsWorldCell(Write.Position.Y) ||
			!VoxelGeneration::IsWorldCell(Write.Position.Z))
		{
			continue;
		}
		NewInfluence += FVector(Write.Position);
		NewInfluence += FVector(Write.Position) + FVector(1.0);
		const FVoxelFeaturePlanKey Key {
			Write.Position,
			Write.Stage
		};

		FVoxelFeaturePlanWrite* Existing =
			NewResolved.Find(Key);

		if (!Existing ||
			Existing->OwnerId <
				Write.OwnerId)
		{
			NewResolved.Add(
				Key,
				Write);
		}
	}
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	VoxelGenerationAllocation::FreeRetainedMap(ResolvedWrites);
	ResolvedWrites = MoveTemp(NewResolved);
	InfluenceBounds = NewInfluence;
	OutError.Reset();
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
void FVoxelFeaturePlan::Finalize()
{
	FString Error;
	const bool bFinalized = Finalize(Error, nullptr);
	checkf(bFinalized, TEXT("%s"), *Error);
}
#endif

void FVoxelFeaturePlan::ReleaseBacking()
{
	VoxelGenerationAllocation::FreeRetainedArray(Writes);
	VoxelGenerationAllocation::FreeRetainedMap(ResolvedWrites);
	InfluenceBounds.Init();
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
	// 根对象由 WorkerShared 授予，此处只计真实 heap backing。
	return VoxelGenerationAllocation::AllocatedBytes(Writes) +
		VoxelGenerationAllocation::AllocatedBytes(ResolvedWrites);
}
