#include "Voxel/Generation/VoxelGenerationPlanCache.h"

bool FVoxelGenerationPlanCache::FindHydrology(const FVoxelHydrologyRegionKey& InKey, FVoxelHydrologyPlanPtr& OutPlan) const
{
	FReadScopeLock Scope(Lock);
	if (const FVoxelHydrologyPlanPtr* Found = Hydrology.Find(InKey))
	{
		OutPlan = *Found;
		return OutPlan.IsValid();
	}
	return false;
}

void FVoxelGenerationPlanCache::StoreHydrology(const FVoxelHydrologyRegionKey& InKey, FVoxelHydrologyPlanPtr InPlan)
{
	FWriteScopeLock Scope(Lock);
	Hydrology.Add(InKey, MoveTemp(InPlan));
}

bool FVoxelGenerationPlanCache::FindCave(const FVoxelGenerationTileKey& InKey, FVoxelCavePlanPtr& OutPlan) const
{
	FReadScopeLock Scope(Lock);
	if (const FVoxelCavePlanPtr* Found = Caves.Find(InKey))
	{
		OutPlan = *Found;
		return OutPlan.IsValid();
	}
	return false;
}

void FVoxelGenerationPlanCache::StoreCave(const FVoxelGenerationTileKey& InKey, FVoxelCavePlanPtr InPlan)
{
	FWriteScopeLock Scope(Lock);
	Caves.Add(InKey, MoveTemp(InPlan));
}

bool FVoxelGenerationPlanCache::FindFeature(const FVoxelGenerationTileKey& InKey, FVoxelFeaturePlanPtr& OutPlan) const
{
	FReadScopeLock Scope(Lock);
	if (const FVoxelFeaturePlanPtr* Found = Features.Find(InKey))
	{
		OutPlan = *Found;
		return OutPlan.IsValid();
	}
	return false;
}

void FVoxelGenerationPlanCache::StoreFeature(const FVoxelGenerationTileKey& InKey, FVoxelFeaturePlanPtr InPlan)
{
	FWriteScopeLock Scope(Lock);
	Features.Add(InKey, MoveTemp(InPlan));
}

bool FVoxelGenerationPlanCache::FindStructure(const FVoxelGenerationTileKey& InKey, FVoxelStructurePlanPtr& OutPlan) const
{
	FReadScopeLock Scope(Lock);
	if (const FVoxelStructurePlanPtr* Found = Structures.Find(InKey))
	{
		OutPlan = *Found;
		return OutPlan.IsValid();
	}
	return false;
}

void FVoxelGenerationPlanCache::StoreStructure(const FVoxelGenerationTileKey& InKey, FVoxelStructurePlanPtr InPlan)
{
	FWriteScopeLock Scope(Lock);
	Structures.Add(InKey, MoveTemp(InPlan));
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
	uint64 Bytes = Hydrology.GetAllocatedSize() + Caves.GetAllocatedSize() + Features.GetAllocatedSize() + Structures.GetAllocatedSize();
	for (const TPair<FVoxelHydrologyRegionKey, FVoxelHydrologyPlanPtr>& Pair : Hydrology)
	{
		if (Pair.Value) Bytes += Pair.Value->Grid.GroundPlane.GetAllocatedSize() + Pair.Value->Grid.RainWeight.GetAllocatedSize() + Pair.Value->Grid.Allowed.GetAllocatedSize();
	}
	for (const TPair<FVoxelGenerationTileKey, FVoxelCavePlanPtr>& Pair : Caves) if (Pair.Value) Bytes += Pair.Value->GetAllocatedBytes();
	for (const TPair<FVoxelGenerationTileKey, FVoxelFeaturePlanPtr>& Pair : Features) if (Pair.Value) Bytes += Pair.Value->GetAllocatedBytes();
	for (const TPair<FVoxelGenerationTileKey, FVoxelStructurePlanPtr>& Pair : Structures) if (Pair.Value) Bytes += Pair.Value->GetAllocatedBytes();
	return Bytes;
}
