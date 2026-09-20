#include "Voxel/Generation/VoxelGenerationPlanCache.h"

#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"

struct FVoxelGenerationPlanCache::FBuildGate
{
	FBuildGate()
	{
		Event =
			FPlatformProcess::
				GetSynchEventFromPool(true);
	}

	~FBuildGate()
	{
		if (Event)
		{
			FPlatformProcess::
				ReturnSynchEventToPool(Event);

			Event = nullptr;
		}
	}

	FEvent* Event = nullptr;
	bool bSuccess = false;
	FString Error;
};

bool FVoxelGenerationPlanCache::FindHydrology(
	const FVoxelHydrologyRegionKey& InKey,
	FVoxelHydrologyPlanPtr& OutPlan) const
{
	FReadScopeLock Scope(Lock);

	if (const FVoxelHydrologyPlanPtr* Found =
		Hydrology.Find(InKey))
	{
		OutPlan = *Found;
		return OutPlan.IsValid();
	}

	return false;
}

void FVoxelGenerationPlanCache::StoreHydrology(
	const FVoxelHydrologyRegionKey& InKey,
	FVoxelHydrologyPlanPtr InPlan)
{
	FWriteScopeLock Scope(Lock);

	Hydrology.Add(
		InKey,
		MoveTemp(InPlan));
}

bool FVoxelGenerationPlanCache::GetOrBuildHydrology(
	const FVoxelHydrologyRegionKey& InKey,
	TFunctionRef<bool(
		FVoxelHydrologyPlan&,
		FString&)> InBuild,
	FVoxelHydrologyPlanPtr& OutPlan,
	FString& OutError)
{
	{
		FReadScopeLock Scope(Lock);

		if (const FVoxelHydrologyPlanPtr* Found =
			Hydrology.Find(InKey))
		{
			OutPlan = *Found;
			OutError.Reset();
			return OutPlan.IsValid();
		}
	}

	TSharedPtr<
		FBuildGate,
		ESPMode::ThreadSafe> Gate;

	bool bOwner = false;

	{
		FWriteScopeLock Scope(Lock);

		if (const FVoxelHydrologyPlanPtr* Found =
			Hydrology.Find(InKey))
		{
			OutPlan = *Found;
			OutError.Reset();
			return OutPlan.IsValid();
		}

		if (TSharedPtr<
			FBuildGate,
			ESPMode::ThreadSafe>* Existing =
				HydrologyBuilds.Find(InKey))
		{
			Gate = *Existing;
		}
		else
		{
			Gate =
				MakeShared<
					FBuildGate,
					ESPMode::ThreadSafe>();

			HydrologyBuilds.Add(
				InKey,
				Gate);

			bOwner = true;
		}
	}

	if (!bOwner)
	{
		Gate->Event->Wait();

		if (!Gate->bSuccess)
		{
			OutError = Gate->Error;
			return false;
		}

		if (!FindHydrology(
			InKey,
			OutPlan))
		{
			OutError =
				TEXT("Hydrology plan build completed without publishing a plan");

			return false;
		}

		OutError.Reset();
		return true;
	}

	FVoxelHydrologyPlan LocalPlan;
	FString BuildError;

	const bool bSuccess =
		InBuild(
			LocalPlan,
			BuildError);

	FVoxelHydrologyPlanPtr BuiltPlan;

	if (bSuccess)
	{
		BuiltPlan =
			MakeShared<
				const FVoxelHydrologyPlan,
				ESPMode::ThreadSafe>(
					MoveTemp(LocalPlan));
	}

	{
		FWriteScopeLock Scope(Lock);

		if (BuiltPlan)
		{
			Hydrology.Add(
				InKey,
				BuiltPlan);
		}

		Gate->bSuccess =
			BuiltPlan.IsValid();

		Gate->Error =
			Gate->bSuccess
				? FString()
				: BuildError;

		HydrologyBuilds.Remove(InKey);
	}

	Gate->Event->Trigger();

	if (!Gate->bSuccess)
	{
		OutError = Gate->Error;
		return false;
	}

	OutPlan = MoveTemp(BuiltPlan);
	OutError.Reset();
	return true;
}

bool FVoxelGenerationPlanCache::FindCave(
	const FVoxelGenerationTileKey& InKey,
	FVoxelCavePlanPtr& OutPlan) const
{
	FReadScopeLock Scope(Lock);

	if (const FVoxelCavePlanPtr* Found =
		Caves.Find(InKey))
	{
		OutPlan = *Found;
		return OutPlan.IsValid();
	}

	return false;
}

void FVoxelGenerationPlanCache::StoreCave(
	const FVoxelGenerationTileKey& InKey,
	FVoxelCavePlanPtr InPlan)
{
	FWriteScopeLock Scope(Lock);

	Caves.Add(
		InKey,
		MoveTemp(InPlan));
}

bool FVoxelGenerationPlanCache::GetOrBuildCave(
	const FVoxelGenerationTileKey& InKey,
	TFunctionRef<bool(
		FVoxelCavePlan&,
		FString&)> InBuild,
	FVoxelCavePlanPtr& OutPlan,
	FString& OutError)
{
	{
		FReadScopeLock Scope(Lock);

		if (const FVoxelCavePlanPtr* Found =
			Caves.Find(InKey))
		{
			OutPlan = *Found;
			OutError.Reset();
			return OutPlan.IsValid();
		}
	}

	TSharedPtr<
		FBuildGate,
		ESPMode::ThreadSafe> Gate;

	bool bOwner = false;

	{
		FWriteScopeLock Scope(Lock);

		if (const FVoxelCavePlanPtr* Found =
			Caves.Find(InKey))
		{
			OutPlan = *Found;
			OutError.Reset();
			return OutPlan.IsValid();
		}

		if (TSharedPtr<
			FBuildGate,
			ESPMode::ThreadSafe>* Existing =
				CaveBuilds.Find(InKey))
		{
			Gate = *Existing;
		}
		else
		{
			Gate =
				MakeShared<
					FBuildGate,
					ESPMode::ThreadSafe>();

			CaveBuilds.Add(
				InKey,
				Gate);

			bOwner = true;
		}
	}

	if (!bOwner)
	{
		Gate->Event->Wait();

		if (!Gate->bSuccess)
		{
			OutError = Gate->Error;
			return false;
		}

		if (!FindCave(
			InKey,
			OutPlan))
		{
			OutError =
				TEXT("Cave plan build completed without publishing a plan");

			return false;
		}

		OutError.Reset();
		return true;
	}

	FVoxelCavePlan LocalPlan;
	FString BuildError;

	const bool bSuccess =
		InBuild(
			LocalPlan,
			BuildError);

	FVoxelCavePlanPtr BuiltPlan;

	if (bSuccess)
	{
		BuiltPlan =
			MakeShared<
				const FVoxelCavePlan,
				ESPMode::ThreadSafe>(
					MoveTemp(LocalPlan));
	}

	{
		FWriteScopeLock Scope(Lock);

		if (BuiltPlan)
		{
			Caves.Add(
				InKey,
				BuiltPlan);
		}

		Gate->bSuccess =
			BuiltPlan.IsValid();

		Gate->Error =
			Gate->bSuccess
				? FString()
				: BuildError;

		CaveBuilds.Remove(InKey);
	}

	Gate->Event->Trigger();

	if (!Gate->bSuccess)
	{
		OutError = Gate->Error;
		return false;
	}

	OutPlan = MoveTemp(BuiltPlan);
	OutError.Reset();
	return true;
}

bool FVoxelGenerationPlanCache::FindFeature(
	const FVoxelGenerationTileKey& InKey,
	FVoxelFeaturePlanPtr& OutPlan) const
{
	FReadScopeLock Scope(Lock);

	if (const FVoxelFeaturePlanPtr* Found =
		Features.Find(InKey))
	{
		OutPlan = *Found;
		return OutPlan.IsValid();
	}

	return false;
}

void FVoxelGenerationPlanCache::StoreFeature(
	const FVoxelGenerationTileKey& InKey,
	FVoxelFeaturePlanPtr InPlan)
{
	FWriteScopeLock Scope(Lock);

	Features.Add(
		InKey,
		MoveTemp(InPlan));
}

bool FVoxelGenerationPlanCache::GetOrBuildFeature(
	const FVoxelGenerationTileKey& InKey,
	TFunctionRef<bool(
		FVoxelFeaturePlan&,
		FString&)> InBuild,
	FVoxelFeaturePlanPtr& OutPlan,
	FString& OutError)
{
	{
		FReadScopeLock Scope(Lock);

		if (const FVoxelFeaturePlanPtr* Found =
			Features.Find(InKey))
		{
			OutPlan = *Found;
			OutError.Reset();
			return OutPlan.IsValid();
		}
	}

	TSharedPtr<
		FBuildGate,
		ESPMode::ThreadSafe> Gate;

	bool bOwner = false;

	{
		FWriteScopeLock Scope(Lock);

		if (const FVoxelFeaturePlanPtr* Found =
			Features.Find(InKey))
		{
			OutPlan = *Found;
			OutError.Reset();
			return OutPlan.IsValid();
		}

		if (TSharedPtr<
			FBuildGate,
			ESPMode::ThreadSafe>* Existing =
				FeatureBuilds.Find(InKey))
		{
			Gate = *Existing;
		}
		else
		{
			Gate =
				MakeShared<
					FBuildGate,
					ESPMode::ThreadSafe>();

			FeatureBuilds.Add(
				InKey,
				Gate);

			bOwner = true;
		}
	}

	if (!bOwner)
	{
		Gate->Event->Wait();

		if (!Gate->bSuccess)
		{
			OutError = Gate->Error;
			return false;
		}

		if (!FindFeature(
			InKey,
			OutPlan))
		{
			OutError =
				TEXT("Feature plan build completed without publishing a plan");

			return false;
		}

		OutError.Reset();
		return true;
	}

	FVoxelFeaturePlan LocalPlan;
	FString BuildError;

	const bool bSuccess =
		InBuild(
			LocalPlan,
			BuildError);

	FVoxelFeaturePlanPtr BuiltPlan;

	if (bSuccess)
	{
		BuiltPlan =
			MakeShared<
				const FVoxelFeaturePlan,
				ESPMode::ThreadSafe>(
					MoveTemp(LocalPlan));
	}

	{
		FWriteScopeLock Scope(Lock);

		if (BuiltPlan)
		{
			Features.Add(
				InKey,
				BuiltPlan);
		}

		Gate->bSuccess =
			BuiltPlan.IsValid();

		Gate->Error =
			Gate->bSuccess
				? FString()
				: BuildError;

		FeatureBuilds.Remove(InKey);
	}

	Gate->Event->Trigger();

	if (!Gate->bSuccess)
	{
		OutError = Gate->Error;
		return false;
	}

	OutPlan = MoveTemp(BuiltPlan);
	OutError.Reset();
	return true;
}

bool FVoxelGenerationPlanCache::FindStructure(
	const FVoxelGenerationTileKey& InKey,
	FVoxelStructurePlanPtr& OutPlan) const
{
	FReadScopeLock Scope(Lock);

	if (const FVoxelStructurePlanPtr* Found =
		Structures.Find(InKey))
	{
		OutPlan = *Found;
		return OutPlan.IsValid();
	}

	return false;
}

void FVoxelGenerationPlanCache::StoreStructure(
	const FVoxelGenerationTileKey& InKey,
	FVoxelStructurePlanPtr InPlan)
{
	FWriteScopeLock Scope(Lock);

	Structures.Add(
		InKey,
		MoveTemp(InPlan));
}

bool FVoxelGenerationPlanCache::GetOrBuildStructure(
	const FVoxelGenerationTileKey& InKey,
	TFunctionRef<bool(
		FVoxelStructurePlan&,
		FString&)> InBuild,
	FVoxelStructurePlanPtr& OutPlan,
	FString& OutError)
{
	{
		FReadScopeLock Scope(Lock);

		if (const FVoxelStructurePlanPtr* Found =
			Structures.Find(InKey))
		{
			OutPlan = *Found;
			OutError.Reset();
			return OutPlan.IsValid();
		}
	}

	TSharedPtr<
		FBuildGate,
		ESPMode::ThreadSafe> Gate;

	bool bOwner = false;

	{
		FWriteScopeLock Scope(Lock);

		if (const FVoxelStructurePlanPtr* Found =
			Structures.Find(InKey))
		{
			OutPlan = *Found;
			OutError.Reset();
			return OutPlan.IsValid();
		}

		if (TSharedPtr<
			FBuildGate,
			ESPMode::ThreadSafe>* Existing =
				StructureBuilds.Find(InKey))
		{
			Gate = *Existing;
		}
		else
		{
			Gate =
				MakeShared<
					FBuildGate,
					ESPMode::ThreadSafe>();

			StructureBuilds.Add(
				InKey,
				Gate);

			bOwner = true;
		}
	}

	if (!bOwner)
	{
		Gate->Event->Wait();

		if (!Gate->bSuccess)
		{
			OutError = Gate->Error;
			return false;
		}

		if (!FindStructure(
			InKey,
			OutPlan))
		{
			OutError =
				TEXT("Structure plan build completed without publishing a plan");

			return false;
		}

		OutError.Reset();
		return true;
	}

	FVoxelStructurePlan LocalPlan;
	FString BuildError;

	const bool bSuccess =
		InBuild(
			LocalPlan,
			BuildError);

	FVoxelStructurePlanPtr BuiltPlan;

	if (bSuccess)
	{
		BuiltPlan =
			MakeShared<
				const FVoxelStructurePlan,
				ESPMode::ThreadSafe>(
					MoveTemp(LocalPlan));
	}

	{
		FWriteScopeLock Scope(Lock);

		if (BuiltPlan)
		{
			Structures.Add(
				InKey,
				BuiltPlan);
		}

		Gate->bSuccess =
			BuiltPlan.IsValid();

		Gate->Error =
			Gate->bSuccess
				? FString()
				: BuildError;

		StructureBuilds.Remove(InKey);
	}

	Gate->Event->Trigger();

	if (!Gate->bSuccess)
	{
		OutError = Gate->Error;
		return false;
	}

	OutPlan = MoveTemp(BuiltPlan);
	OutError.Reset();
	return true;
}

void FVoxelGenerationPlanCache::Reset()
{
	FWriteScopeLock Scope(Lock);

	Hydrology.Reset();
	Caves.Reset();
	Features.Reset();
	Structures.Reset();
}

uint64 FVoxelGenerationPlanCache::GetAllocatedBytes() const
{
	FReadScopeLock Scope(Lock);

	uint64 Bytes =
		Hydrology.GetAllocatedSize() +
		Caves.GetAllocatedSize() +
		Features.GetAllocatedSize() +
		Structures.GetAllocatedSize();

	for (const TPair<
		FVoxelHydrologyRegionKey,
		FVoxelHydrologyPlanPtr>& Pair :
		Hydrology)
	{
		if (Pair.Value)
		{
			Bytes +=
				Pair.Value->
					GetAllocatedBytes();
		}
	}

	for (const TPair<
		FVoxelGenerationTileKey,
		FVoxelCavePlanPtr>& Pair :
		Caves)
	{
		if (Pair.Value)
		{
			Bytes +=
				Pair.Value->
					GetAllocatedBytes();
		}
	}

	for (const TPair<
		FVoxelGenerationTileKey,
		FVoxelFeaturePlanPtr>& Pair :
		Features)
	{
		if (Pair.Value)
		{
			Bytes +=
				Pair.Value->
					GetAllocatedBytes();
		}
	}

	for (const TPair<
		FVoxelGenerationTileKey,
		FVoxelStructurePlanPtr>& Pair :
		Structures)
	{
		if (Pair.Value)
		{
			Bytes +=
				Pair.Value->
					GetAllocatedBytes();
		}
	}

	return Bytes;
}
