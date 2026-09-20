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

template<typename KeyType, typename ValueType>
bool FVoxelGenerationPlanCache::GetOrBuildNatural(
	const KeyType& InKey,
	TMap<KeyType, TSharedPtr<const ValueType, ESPMode::ThreadSafe>>& InValues,
	TMap<KeyType, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>>& InBuilds,
	TFunctionRef<bool(ValueType&, FString&)> InBuild,
	TSharedPtr<const ValueType, ESPMode::ThreadSafe>& OutValue,
	FString& OutError)
{
	{
		FReadScopeLock Scope(NaturalLock);
		if (const TSharedPtr<const ValueType, ESPMode::ThreadSafe>* Found = InValues.Find(InKey))
		{
			OutValue = *Found;
			OutError.Reset();
			return OutValue.IsValid();
		}
	}

	TSharedPtr<FBuildGate, ESPMode::ThreadSafe> Gate;
	bool bOwner = false;
	{
		FWriteScopeLock Scope(NaturalLock);
		if (const TSharedPtr<const ValueType, ESPMode::ThreadSafe>* Found = InValues.Find(InKey))
		{
			OutValue = *Found;
			OutError.Reset();
			return OutValue.IsValid();
		}

		if (TSharedPtr<FBuildGate, ESPMode::ThreadSafe>* Existing = InBuilds.Find(InKey))
		{
			Gate = *Existing;
		}
		else
		{
			Gate = MakeShared<FBuildGate, ESPMode::ThreadSafe>();
			InBuilds.Add(InKey, Gate);
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

		FReadScopeLock Scope(NaturalLock);
		if (const TSharedPtr<const ValueType, ESPMode::ThreadSafe>* Found = InValues.Find(InKey))
		{
			OutValue = *Found;
			OutError.Reset();
			return OutValue.IsValid();
		}

		OutError = TEXT("Natural generation build completed without publishing a value");
		return false;
	}

	ValueType LocalValue;
	FString BuildError;
	const bool bSuccess = InBuild(LocalValue, BuildError);
	TSharedPtr<const ValueType, ESPMode::ThreadSafe> BuiltValue;
	if (bSuccess)
	{
		BuiltValue = MakeShared<const ValueType, ESPMode::ThreadSafe>(MoveTemp(LocalValue));
	}

	{
		FWriteScopeLock Scope(NaturalLock);
		if (BuiltValue)
		{
			InValues.Add(InKey, BuiltValue);
		}
		Gate->bSuccess = BuiltValue.IsValid();
		Gate->Error = Gate->bSuccess ? FString() : BuildError;
		InBuilds.Remove(InKey);
	}
	Gate->Event->Trigger();

	if (!Gate->bSuccess)
	{
		OutError = Gate->Error;
		return false;
	}

	OutValue = MoveTemp(BuiltValue);
	OutError.Reset();
	return true;
}

bool FVoxelGenerationPlanCache::GetOrBuildBaseColumn(
	const FIntPoint& InPosition,
	TFunctionRef<bool(FVoxelBaseColumnEntry&, FString&)> InBuild,
	FVoxelBaseColumnEntryPtr& OutEntry,
	FString& OutError)
{
	return GetOrBuildNatural(InPosition, BaseColumns, BaseColumnBuilds, InBuild, OutEntry, OutError);
}

bool FVoxelGenerationPlanCache::GetOrBuildRiverField(
	const FVoxelNaturalTileKey& InKey,
	TFunctionRef<bool(FVoxelRiverFieldTile&, FString&)> InBuild,
	FVoxelRiverFieldTilePtr& OutTile,
	FString& OutError)
{
	return GetOrBuildNatural(InKey, RiverFields, RiverFieldBuilds, InBuild, OutTile, OutError);
}

bool FVoxelGenerationPlanCache::GetOrBuildLake(
	const FVoxelLakeAnchorKey& InKey,
	TFunctionRef<bool(FVoxelLakeAnchorPlan&, FString&)> InBuild,
	FVoxelLakeAnchorPlanPtr& OutPlan,
	FString& OutError)
{
	return GetOrBuildNatural(InKey, Lakes, LakeBuilds, InBuild, OutPlan, OutError);
}

bool FVoxelGenerationPlanCache::GetOrBuildNaturalColumn(
	const FIntPoint& InPosition,
	TFunctionRef<bool(FVoxelNaturalColumnEntry&, FString&)> InBuild,
	FVoxelNaturalColumnEntryPtr& OutEntry,
	FString& OutError)
{
	return GetOrBuildNatural(InPosition, NaturalColumns, NaturalColumnBuilds, InBuild, OutEntry, OutError);
}

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
	{
		FWriteScopeLock Scope(NaturalLock);
		BaseColumns.Reset();
		RiverFields.Reset();
		Lakes.Reset();
		NaturalColumns.Reset();
		BaseColumnBuilds.Reset();
		RiverFieldBuilds.Reset();
		LakeBuilds.Reset();
		NaturalColumnBuilds.Reset();
	}

	FWriteScopeLock Scope(Lock);

	Hydrology.Reset();
	Caves.Reset();
	Features.Reset();
	Structures.Reset();
}

void FVoxelGenerationPlanCache::TrimNaturalCaches(
	TConstArrayView<FIntPoint> InCenters,
	const int32 InKeepRadiusCells)
{
	if (InCenters.IsEmpty() || InKeepRadiusCells < 0)
	{
		return;
	}

	auto IsRetained = [InCenters, InKeepRadiusCells](const FIntPoint& InPosition)
	{
		for (const FIntPoint& Center : InCenters)
		{
			if (FMath::Abs(InPosition.X - Center.X) <= InKeepRadiusCells &&
				FMath::Abs(InPosition.Y - Center.Y) <= InKeepRadiusCells)
			{
				return true;
			}
		}
		return false;
	};

	FWriteScopeLock Scope(NaturalLock);
	for (auto It = BaseColumns.CreateIterator(); It; ++It)
	{
		if (!IsRetained(It.Key())) It.RemoveCurrent();
	}
	for (auto It = NaturalColumns.CreateIterator(); It; ++It)
	{
		if (!IsRetained(It.Key())) It.RemoveCurrent();
	}
	for (auto It = RiverFields.CreateIterator(); It; ++It)
	{
		if (!IsRetained(It.Key().Coordinate * FVoxelRiverFieldTile::Side)) It.RemoveCurrent();
	}
	for (auto It = Lakes.CreateIterator(); It; ++It)
	{
		if (!IsRetained(It.Key().Coordinate * 64)) It.RemoveCurrent();
	}
}

uint64 FVoxelGenerationPlanCache::GetAllocatedBytes() const
{
	uint64 Bytes = 0;

	{
		FReadScopeLock NaturalScope(NaturalLock);
		Bytes += BaseColumns.GetAllocatedSize() +
			RiverFields.GetAllocatedSize() +
			Lakes.GetAllocatedSize() +
			NaturalColumns.GetAllocatedSize();
		for (const TPair<FIntPoint, FVoxelBaseColumnEntryPtr>& Pair : BaseColumns)
		{
			if (Pair.Value) Bytes += Pair.Value->GetAllocatedBytes();
		}
		for (const TPair<FVoxelNaturalTileKey, FVoxelRiverFieldTilePtr>& Pair : RiverFields)
		{
			if (Pair.Value) Bytes += Pair.Value->GetAllocatedBytes();
		}
		for (const TPair<FVoxelLakeAnchorKey, FVoxelLakeAnchorPlanPtr>& Pair : Lakes)
		{
			if (Pair.Value) Bytes += Pair.Value->GetAllocatedBytes();
		}
		for (const TPair<FIntPoint, FVoxelNaturalColumnEntryPtr>& Pair : NaturalColumns)
		{
			if (Pair.Value) Bytes += Pair.Value->GetAllocatedBytes();
		}
	}

	FReadScopeLock Scope(Lock);
	Bytes +=
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
