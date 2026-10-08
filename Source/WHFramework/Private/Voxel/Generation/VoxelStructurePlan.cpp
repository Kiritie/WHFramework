#include "Voxel/Generation/VoxelStructurePlan.h"

#include "Misc/ScopeExit.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

bool FVoxelStructurePlan::Finalize(FString& OutError, const TAtomic<bool>* InCancel)
{
	const auto* Allocation = FVoxelGenerationAllocationScope::Get();
	if (!(Allocation ? Allocation->Check(OutError) : FVoxelGenerationAllocationScope::AllowPassive(OutError)))
	{
		return false;
	}
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	FBox NewInfluenceBounds(ForceInit);
	for (const FVoxelStructurePlanClear& Clear : Clears)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		NewInfluenceBounds += FBox(FVector(Clear.Bounds.Min), FVector(Clear.Bounds.Max));
	}

	TMap<FVoxelStructurePlanKey, FVoxelStructurePlanWrite> Resolved;
	ON_SCOPE_EXIT
	{
		VoxelGenerationAllocation::FreeRetainedMap(Resolved);
	};
	if (!VoxelGenerationAllocation::RetainMap(Resolved, Writes.Num(), OutError))
	{
		return false;
	}

	for (const FVoxelStructurePlanWrite& Write :
		Writes)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		NewInfluenceBounds += FVector(Write.Position);
		NewInfluenceBounds += FVector(Write.Position) + FVector(1.0);
		const FVoxelStructurePlanKey Key {
			Write.Position,
			Write.Stage
		};

		FVoxelStructurePlanWrite* Existing =
			Resolved.Find(Key);

		if (!Existing ||
			Existing->OwnerId <
				Write.OwnerId)
		{
			Resolved.Add(
				Key,
				Write);
		}
	}
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	const uint64 OldBytes = VoxelGenerationAllocation::AllocatedBytes(ResolvedWrites);
	ResolvedWrites = MoveTemp(Resolved);
	if (auto* Scope = FVoxelGenerationAllocationScope::Get())
	{
		Scope->ReleaseRetainedBytes(OldBytes);
	}
	InfluenceBounds = NewInfluenceBounds;
	OutError.Reset();
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
void FVoxelStructurePlan::Finalize()
{
	FString Error;
	const bool bFinalized = Finalize(Error);
	checkf(bFinalized, TEXT("Structure fixture finalization failed: %s"), *Error);
}
#endif

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
		VoxelGenerationAllocation::AllocatedBytes(Clears) +
		VoxelGenerationAllocation::AllocatedBytes(Writes) +
		VoxelGenerationAllocation::AllocatedBytes(Details) +
		VoxelGenerationAllocation::AllocatedBytes(ResolvedWrites);
}
