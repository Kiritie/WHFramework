#include "Voxel/Streaming/VoxelInterestManager.h"

#include "Voxel/Task/VoxelBoundedAllocation.h"

namespace
{
	bool GrantInterestInput(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources,
		const FVoxelGenerationConsumerKey& Consumer, const uint64 Bytes,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease, const TCHAR* Stage,
		FString& OutError, const uint64 RequiredInputBytes = 0, const int32 RequiredLeaseCount = 0)
	{
		const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, OutLease,
			0, RequiredInputBytes, RequiredLeaseCount);
		if (Admission == EVoxelTerrainResourceAdmission::Granted) return true;
		const auto State = Resources->Snapshot();
		const TCHAR* Reason = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked") :
			Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable") : TEXT("InvalidRequest");
		OutError = FString::Printf(TEXT("%s: interest %s request=%llu requiredInput=%llu requiredLeases=%d used=%llu limit=%llu leases=%d releaseVersion=%llu"),
			Reason, Stage, Bytes, RequiredInputBytes, RequiredLeaseCount, State.Used[int32(EVoxelTerrainResourcePool::Input)],
			State.Limits.InputBytes, State.LeaseCount, State.ReleaseVersion);
		return false;
	}
}

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

uint64 FVoxelInterestSet::GetFinalizationAllocatedBytes() const
{
	return Admissions.GetAllocatedSize() + AdmissionPriorities.GetAllocatedSize() +
		AdmissionLanes[0].GetAllocatedSize() + AdmissionLanes[1].GetAllocatedSize() +
		AdmissionLanes[2].GetAllocatedSize() + AdmissionLanes[3].GetAllocatedSize() +
		ExactOrder.GetAllocatedSize() + ExactAdmissionLanes[0].GetAllocatedSize() + ExactAdmissionLanes[1].GetAllocatedSize() +
		PlayableFineKeys.GetAllocatedSize() + FinePagePriorities.GetAllocatedSize() + FinePageServicePriorities.GetAllocatedSize() +
		GeometryServicePriorities.GetAllocatedSize() + CriticalFinePages.GetAllocatedSize() + CriticalDependencies.GetAllocatedSize() +
		VoxelProxyPriorities.GetAllocatedSize() + SurfacePriorities.GetAllocatedSize() + MacroPriorities.GetAllocatedSize();
}

bool FVoxelInterestManager::CopyForPriority(const FVoxelInterestSet& InPrevious,
	FVoxelInterestSet& OutInterest, FString& OutError) const
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 Bytes = FMath::Max<uint64>(1, InPrevious.GetOwnedAllocatedBytes());
	if (Resources)
	{
		if (!GrantInterestInput(Resources.ToSharedRef(), Consumer, Bytes, Lease, TEXT("priority copy"), OutError)) return false;
	}
	OutInterest = InPrevious;
	OutInterest.DemandLease.Reset(); OutInterest.CompositionLease.Reset();
	OutInterest.FinalizationLease.Reset(); OutInterest.CoarseCompositionLease.Reset();
	for (auto& Existing : OutInterest.CoarsePlanLeases) Existing.Reset();
	OutInterest.CopyLease = MoveTemp(Lease);
	if (Resources && OutInterest.GetOwnedAllocatedBytes() > Bytes)
	{
		OutError = FString::Printf(TEXT("TerminalFailure: priority interest copy actual=%llu granted=%llu"),
			OutInterest.GetOwnedAllocatedBytes(), Bytes);
		return false;
	}
	int32 FineCount = 0;
	for (const auto& Pair : OutInterest.Exact) FineCount += Pair.Value.bFineRender ? 1 : 0;
	// 复制只授予旧容量；重排前为可能增长的 Map、Set 和 lane 单独授予完整上界。
	if (!ReserveFinalization(OutInterest, FineCount, OutError)) return false;
	OutError.Reset();
	return true;
}

bool FVoxelInterestManager::ReserveFinalization(FVoxelInterestSet& Interest, const int32 FineCount, FString& OutError) const
{
	const int64 Rows64 = int64(FineCount) + Interest.VoxelProxy.Num() + Interest.Surface.Num() + Interest.Macro.Num();
	if (Rows64 > MAX_int32)
	{ OutError = TEXT("Oversized: terrain admission rows"); return false; }
	const int32 Rows = FMath::Max(int32(Rows64), Interest.Admissions.Num());
	const int32 Pages = Interest.FinePagePlan ? Interest.FinePagePlan->Pages.Num() : 0;
	const auto Capacity = [](const uint64 Existing, const uint64 Required) { return FMath::Max(Existing, Required); };
	uint64 Bytes = Capacity(Interest.Admissions.GetAllocatedSize(), VoxelBoundedAllocation::ArrayBytes<FVoxelViewAdmission>(Rows)) +
		Capacity(Interest.AdmissionPriorities.GetAllocatedSize(), VoxelBoundedAllocation::ArrayBytes<int32>(Rows)) +
		Capacity(Interest.ExactOrder.GetAllocatedSize(), VoxelBoundedAllocation::ArrayBytes<FIntVector>(Interest.Exact.Num())) +
		Capacity(Interest.PlayableFineKeys.GetAllocatedSize(), VoxelBoundedAllocation::ArrayBytes<FIntVector>(FineCount)) +
		Capacity(Interest.FinePagePriorities.GetAllocatedSize(), VoxelBoundedAllocation::MapBytes<FVoxelFinePageKey, int32>(Pages)) +
		Capacity(Interest.FinePageServicePriorities.GetAllocatedSize(), VoxelBoundedAllocation::MapBytes<FVoxelFinePageKey, FVoxelTerrainServicePriority>(Pages)) +
		Capacity(Interest.GeometryServicePriorities.GetAllocatedSize(), VoxelBoundedAllocation::MapBytes<FVoxelTerrainSpatialKey, FVoxelTerrainServicePriority>(Interest.Geometry.Num())) +
		Capacity(Interest.CriticalFinePages.GetAllocatedSize(), VoxelBoundedAllocation::SetBytes<FVoxelFinePageKey>(Pages)) +
		Capacity(Interest.CriticalDependencies.GetAllocatedSize(), VoxelBoundedAllocation::SetBytes<FVoxelTerrainSpatialKey>(Interest.Geometry.Num())) +
		Capacity(Interest.VoxelProxyPriorities.GetAllocatedSize(), VoxelBoundedAllocation::MapBytes<FVoxelViewKey, int32>(Interest.VoxelProxy.Num())) +
		Capacity(Interest.SurfacePriorities.GetAllocatedSize(), VoxelBoundedAllocation::MapBytes<FVoxelSurfaceTileKey, int32>(Interest.Surface.Num())) +
		Capacity(Interest.MacroPriorities.GetAllocatedSize(), VoxelBoundedAllocation::MapBytes<FVoxelMacroTileKey, int32>(Interest.Macro.Num()));
	for (const auto& Lane : Interest.AdmissionLanes)
		Bytes += Capacity(Lane.GetAllocatedSize(), VoxelBoundedAllocation::ArrayBytes<int32>(Rows));
	for (const auto& Lane : Interest.ExactAdmissionLanes)
		Bytes += Capacity(Lane.GetAllocatedSize(), VoxelBoundedAllocation::ArrayBytes<int32>(Interest.Exact.Num()));
	if (Resources)
	{
		if (Interest.FinalizationLease)
		{ OutError = TEXT("TerminalFailure: interest finalization already owns a reservation"); return false; }
		const uint64 RequiredInput = Interest.CopyLease ? Interest.CopyLease->Bytes() : 0;
		if (!GrantInterestInput(Resources.ToSharedRef(), Consumer, FMath::Max<uint64>(1, Bytes), Interest.FinalizationLease,
			TEXT("finalization containers"), OutError, RequiredInput, Interest.CopyLease ? 1 : 0)) return false;
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

bool FVoxelInterestManager::SettleFinalization(FVoxelInterestSet& Interest, FString& OutError)
{
	if (!Interest.FinalizationLease) return true;
	const uint64 Actual = Interest.GetFinalizationAllocatedBytes();
	const uint64 Granted = Interest.FinalizationLease->Bytes();
	if (Actual > Granted)
	{
		OutError = FString::Printf(TEXT("TerminalFailure: interest finalization actual=%llu granted=%llu owned=%llu accounted=%llu"),
			Actual, Granted, Interest.GetOwnedAllocatedBytes(), Interest.GetResourceAccountedBytes());
		return false;
	}
	if (Interest.CopyLease)
	{
		const uint64 OwnedBeforeSettlement = Interest.GetOwnedAllocatedBytes();
		if (Actual > OwnedBeforeSettlement)
		{
			OutError = FString::Printf(TEXT("TerminalFailure: interest ownership subset actual=%llu owned=%llu"), Actual, OwnedBeforeSettlement);
			return false;
		}
		const uint64 CopyActual = OwnedBeforeSettlement - Actual;
		if (CopyActual > Interest.CopyLease->Bytes())
		{
			OutError = FString::Printf(TEXT("TerminalFailure: priority interest retained actual=%llu granted=%llu"),
				CopyActual, Interest.CopyLease->Bytes());
			return false;
		}
		// 新快照的两个租约各自承担实际拥有的容器；不把旧副本授额重复长期占用。
		if (!Interest.CopyLease->Resize(FMath::Max<uint64>(1, CopyActual)))
		{
			OutError = TEXT("TerminalFailure: priority interest copy settlement");
			return false;
		}
	}
	if (!Interest.FinalizationLease->Resize(FMath::Max<uint64>(1, Actual)))
	{
		OutError = TEXT("TerminalFailure: interest finalization settlement");
		return false;
	}
	const uint64 Owned = Interest.GetOwnedAllocatedBytes();
	const uint64 Accounted = Interest.GetResourceAccountedBytes();
	if (Owned > Accounted)
	{
		OutError = FString::Printf(TEXT("TerminalFailure: interest owned allocation actual=%llu accounted=%llu finalization=%llu copy=%llu composition=%llu coarse=%llu"),
			Owned, Accounted, Actual, Interest.CopyLease ? Interest.CopyLease->Bytes() : 0,
			Interest.CompositionLease ? Interest.CompositionLease->Bytes() : 0,
			Interest.CoarseCompositionLease ? Interest.CoarseCompositionLease->Bytes() : 0);
		return false;
	}
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
		if (!GrantInterestInput(Resources.ToSharedRef(), Consumer, Bytes, Lease, TEXT("publication inputs"), OutError)) return false;
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
	const uint64 ActualBytes = Inputs->GetAllocatedBytes() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainPublicationInputs>();
	if (ActualBytes > Bytes)
	{
		OutError = FString::Printf(TEXT("TerminalFailure: terrain publication input actual=%llu granted=%llu"), ActualBytes, Bytes);
		return false;
	}
	if (Lease && !Lease->Resize(FMath::Max<uint64>(1, ActualBytes)))
	{
		OutError = TEXT("TerminalFailure: terrain publication input settlement");
		return false;
	}
	Interest.PublicationInputs = MoveTemp(Inputs);
	OutError.Reset();
	return true;
}
