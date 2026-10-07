#include "Voxel/Streaming/VoxelInterestManager.h"

#include "Voxel/Task/VoxelBoundedAllocation.h"

uint64 FVoxelInterestSet::GetOwnedAllocatedBytes() const
{
	return GetAllocatedBytes() - (PublicationInputs ? PublicationInputs->GetAllocatedBytes() : 0) -
		(FinePagePlan ? FinePagePlan->Pages.GetAllocatedSize() + FinePagePlan->SectionOwners.GetAllocatedSize() : 0) -
		(FineSections ? FineSections->GetAllocatedSize() : 0) + PlanningError.GetAllocatedSize();
}

uint64 FVoxelInterestSet::GetResourceAccountedBytes() const
{
	const FVoxelTerrainResourceLease* Seen[8] = {};
	int32 Count = 0;
	uint64 Bytes = 0;
	const auto Add = [&](const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
	{
		if (!Lease) return;
		for (int32 Index = 0; Index < Count; ++Index) if (Seen[Index] == Lease.Get()) return;
		check(Count < UE_ARRAY_COUNT(Seen));
		Seen[Count++] = Lease.Get();
		Bytes += Lease->Bytes();
	};
	Add(DemandLease); Add(CompositionLease); Add(CopyLease); Add(FinalizationLease); Add(CoarseCompositionLease);
	for (const auto& Lease : CoarsePlanLeases) Add(Lease);
	return Bytes;
}

bool FVoxelInterestManager::CopyForPriority(const FVoxelInterestSet& InPrevious,
	FVoxelInterestSet& OutInterest, FString& OutError) const
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 Bytes = FMath::Max<uint64>(1, InPrevious.GetOwnedAllocatedBytes());
	if (Resources)
	{
		if (Bytes > Resources->Snapshot().Limits.InputBytes)
		{ OutError = TEXT("Oversized: priority interest copy"); return false; }
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, Lease))
		{ OutError = TEXT("CapacityBlocked: priority interest copy"); return false; }
	}
	OutInterest = InPrevious;
	OutInterest.DemandLease.Reset(); OutInterest.CompositionLease.Reset();
	OutInterest.FinalizationLease.Reset(); OutInterest.CoarseCompositionLease.Reset();
	for (auto& Existing : OutInterest.CoarsePlanLeases) Existing.Reset();
	OutInterest.CopyLease = MoveTemp(Lease);
	check(!Resources || OutInterest.GetOwnedAllocatedBytes() <= Bytes);
	OutError.Reset();
	return true;
}

bool FVoxelInterestManager::ReserveFinalization(FVoxelInterestSet& Interest, const int32 FineCount, FString& OutError) const
{
	const int64 Rows64 = int64(FineCount) + Interest.VoxelProxy.Num() + Interest.Surface.Num() + Interest.Macro.Num();
	if (Rows64 > MAX_int32)
	{ OutError = TEXT("Oversized: terrain admission rows"); return false; }
	const int32 Rows = int32(Rows64);
	const int32 Pages = Interest.FinePagePlan->Pages.Num();
	const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<FVoxelViewAdmission>(Rows) +
		5 * VoxelBoundedAllocation::ArrayBytes<int32>(Rows) +
		VoxelBoundedAllocation::ArrayBytes<FIntVector>(Interest.Exact.Num()) +
		2 * VoxelBoundedAllocation::ArrayBytes<int32>(Interest.Exact.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FIntVector>(FineCount) +
		VoxelBoundedAllocation::MapBytes<FVoxelFinePageKey, int32>(Pages) +
		VoxelBoundedAllocation::MapBytes<FVoxelFinePageKey, FVoxelTerrainServicePriority>(Pages) +
		VoxelBoundedAllocation::MapBytes<FVoxelTerrainSpatialKey, FVoxelTerrainServicePriority>(Interest.Geometry.Num()) +
		VoxelBoundedAllocation::SetBytes<FVoxelFinePageKey>(Pages) +
		VoxelBoundedAllocation::SetBytes<FVoxelTerrainSpatialKey>(Interest.Geometry.Num()) +
		VoxelBoundedAllocation::MapBytes<FVoxelViewKey, int32>(Interest.VoxelProxy.Num()) +
		VoxelBoundedAllocation::MapBytes<FVoxelSurfaceTileKey, int32>(Interest.Surface.Num()) +
		VoxelBoundedAllocation::MapBytes<FVoxelMacroTileKey, int32>(Interest.Macro.Num());
	if (Resources)
	{
		if (Bytes > Resources->Snapshot().Limits.InputBytes)
		{ OutError = TEXT("Oversized: terrain finalization containers"); return false; }
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
			FMath::Max<uint64>(1, Bytes), Interest.FinalizationLease))
		{ OutError = TEXT("CapacityBlocked: terrain finalization containers"); return false; }
	}
	Interest.Admissions.Reserve(Rows); Interest.AdmissionPriorities.Reserve(Rows);
	for (auto& Lane : Interest.AdmissionLanes) Lane.Reserve(Rows);
	Interest.ExactOrder.Reserve(Interest.Exact.Num());
	for (auto& Lane : Interest.ExactAdmissionLanes) Lane.Reserve(Interest.Exact.Num());
	Interest.PlayableFineKeys.Reserve(FineCount);
	Interest.FinePagePriorities.Reserve(Pages); Interest.FinePageServicePriorities.Reserve(Pages);
	Interest.GeometryServicePriorities.Reserve(Interest.Geometry.Num());
	Interest.CriticalFinePages.Reserve(Pages); Interest.CriticalDependencies.Reserve(Interest.Geometry.Num());
	Interest.VoxelProxyPriorities.Reserve(Interest.VoxelProxy.Num());
	Interest.SurfacePriorities.Reserve(Interest.Surface.Num()); Interest.MacroPriorities.Reserve(Interest.Macro.Num());
	OutError.Reset();
	return true;
}

bool FVoxelInterestManager::BuildPublicationInputs(FVoxelInterestSet& Interest, FString& OutError) const
{
	int64 FeedbackRows = Interest.Exact.Num();
	for (const auto& Pair : Interest.FineShardDemands)
	{
		const int64 Axis = Pair.Key.SideCells / 16;
		FeedbackRows += Axis * Axis * Axis;
	}
	if (FeedbackRows > MAX_int32)
	{ OutError = TEXT("Oversized: Fine feedback section index"); return false; }
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainPublicationInputs>() +
		VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainGeometry>(Interest.Geometry.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelViewKey>(Interest.TerrainPlan.Roots.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FIntVector>(Interest.FineSections->Num()) +
		VoxelBoundedAllocation::ArrayBytes<FIntVector>(Interest.Exact.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelViewKey>(Interest.VoxelProxy.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelSurfaceTileKey>(Interest.Surface.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelMacroTileKey>(Interest.Macro.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelFinePageKey>(Interest.FinePagePlan->Pages.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelFineShardKey>(Interest.FineShardDemands.Num()) +
		VoxelBoundedAllocation::ArrayBytes<TPair<FIntVector, FVoxelFinePageKey>>(Interest.FinePagePlan->SectionOwners.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelViewAdmission>(Interest.Admissions.Num()) +
		VoxelBoundedAllocation::ArrayBytes<int32>(Interest.AdmissionPriorities.Num()) +
		VoxelBoundedAllocation::ArrayBytes<int32>(Interest.AdmissionLanes[0].Num()) +
		VoxelBoundedAllocation::ArrayBytes<int32>(Interest.AdmissionLanes[1].Num()) +
		VoxelBoundedAllocation::ArrayBytes<int32>(Interest.AdmissionLanes[2].Num()) +
		VoxelBoundedAllocation::ArrayBytes<int32>(Interest.AdmissionLanes[3].Num());
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (Resources)
	{
		if (Bytes > Resources->Snapshot().Limits.InputBytes)
		{ OutError = TEXT("Oversized: terrain publication inputs"); return false; }
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, Lease))
		{ OutError = TEXT("CapacityBlocked: terrain publication inputs"); return false; }
	}
	auto Inputs = Lease ? VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainPublicationInputs>(Lease) :
		MakeShared<FVoxelTerrainPublicationInputs, ESPMode::ThreadSafe>();
	Inputs->MaximumFeedbackSections = int32(FeedbackRows);
	Inputs->Geometry.Reserve(Interest.Geometry.Num());
	for (const auto& Pair : Interest.Geometry) Inputs->Geometry.Add(Pair.Value);
	Inputs->Roots.Reserve(Interest.TerrainPlan.Roots.Num());
	for (const auto& Key : Interest.TerrainPlan.Roots) Inputs->Roots.Add(Key);
	Inputs->Fine.Reserve(Interest.FineSections->Num());
	for (const auto& Key : *Interest.FineSections) Inputs->Fine.Add(Key);
	Inputs->RuntimeSections.Reserve(Interest.Exact.Num());
	for (const auto& Pair : Interest.Exact) Inputs->RuntimeSections.Add(Pair.Key);
	Inputs->Proxy.Reserve(Interest.VoxelProxy.Num());
	for (const auto& Key : Interest.VoxelProxy) Inputs->Proxy.Add(Key);
	Inputs->Surface.Reserve(Interest.Surface.Num());
	for (const auto& Key : Interest.Surface) Inputs->Surface.Add(Key);
	Inputs->Macro.Reserve(Interest.Macro.Num());
	for (const auto& Key : Interest.Macro) Inputs->Macro.Add(Key);
	Inputs->FinePages.Reserve(Interest.FinePagePlan->Pages.Num());
	for (const auto& Key : Interest.FinePagePlan->Pages) Inputs->FinePages.Add(Key);
	Inputs->FineShards.Reserve(Interest.FineShardDemands.Num());
	for (const auto& Pair : Interest.FineShardDemands) Inputs->FineShards.Add(Pair.Key);
	Inputs->FineOwners.Reserve(Interest.FinePagePlan->SectionOwners.Num());
	for (const auto& Pair : Interest.FinePagePlan->SectionOwners) Inputs->FineOwners.Add(Pair);
	Inputs->Admissions.Reserve(Interest.Admissions.Num()); Inputs->Admissions.Append(Interest.Admissions);
	Inputs->AdmissionPriorities.Reserve(Interest.AdmissionPriorities.Num()); Inputs->AdmissionPriorities.Append(Interest.AdmissionPriorities);
	for (int32 Lane = 0; Lane < 4; ++Lane)
	{
		Inputs->AdmissionLanes[Lane].Reserve(Interest.AdmissionLanes[Lane].Num());
		Inputs->AdmissionLanes[Lane].Append(Interest.AdmissionLanes[Lane]);
	}
	check(Inputs->GetAllocatedBytes() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainPublicationInputs>() <= Bytes);
	Interest.PublicationInputs = MoveTemp(Inputs);
	OutError.Reset();
	return true;
}
