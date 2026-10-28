#include "Voxel/Rendering/VoxelViewManager.h"

#include "Misc/ScopeExit.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/VoxelModule.h"

bool FVoxelViewManager::IsRetirementWanted(const FVoxelPublishGroupKey& InCandidate) const
{
	const auto WantedByRoot = [&InCandidate](const FInterestInputRoot* Root)
	{
		if (!Root) return false;
		switch (InCandidate.Representation)
		{
		case 0: return Root->FinePagePlan.Pages.Contains({InCandidate.Coordinate, InCandidate.Level});
		case 4: return Root->FineWanted.Contains(InCandidate.Coordinate);
		case 1: return Root->ProxyWanted.Contains({InCandidate.Coordinate, InCandidate.Level});
		case 2: return Root->SurfaceWanted.Contains({FIntPoint(InCandidate.Coordinate.X, InCandidate.Coordinate.Y), InCandidate.Level});
		case 3: return Root->MacroWanted.Contains({FIntPoint(InCandidate.Coordinate.X, InCandidate.Coordinate.Y), InCandidate.Level});
		default: return false;
		}
	};
	if (WantedByRoot(AppliedInputRoot ? AppliedInputRoot.Get() : &EmptyInputRoot) ||
		(InterestWorkspace && WantedByRoot(InterestWorkspace->Root.Get()))) return true;
	const auto Desired = Module.GetDesiredInterestSnapshot();
	if (!Desired) return false;
	switch (InCandidate.Representation)
	{
	case 0: return Desired->FinePagePlan && Desired->FinePagePlan->Pages.Contains({InCandidate.Coordinate, InCandidate.Level});
	case 4: return Desired->FineSections && Desired->FineSections->Contains(InCandidate.Coordinate);
	case 1: return Desired->VoxelProxy.Contains({InCandidate.Coordinate, InCandidate.Level});
	case 2: return Desired->Surface.Contains({FIntPoint(InCandidate.Coordinate.X, InCandidate.Coordinate.Y), InCandidate.Level});
	case 3: return Desired->Macro.Contains({FIntPoint(InCandidate.Coordinate.X, InCandidate.Coordinate.Y), InCandidate.Level});
	default: return false;
	}
}

void FVoxelViewManager::ResetRetirementContinuation()
{
	RetirementContinuation.Reset();
	RetirementInterestSnapshot.Reset();
	ReleaseInputRoot(RetirementInputRoot);
	RetirementCoverageRoot.Reset();
	RetirementHeightfieldRoot.Reset();
	RetirementContentSnapshot = {};
	RetirementHierarchyResources.Reset();
	RetirementHierarchyConsumer = {};
	RetirementHierarchyAdmission = EVoxelTerrainResourceAdmission::Granted;
	RetirementHierarchyCapacityVersion = 0;
	RetirementScopeRects.Empty();
	RetirementScopeLease.Reset();
	bRetirementReadScope = false;
}

void FVoxelViewManager::CleanupRetiredRepresentations(const double InNow)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewRetire);
	const double Start = FPlatformTime::Seconds();
	const double Deadline = TerrainDeadline(0.25);
	ON_SCOPE_EXIT { LastRetireMilliseconds = (FPlatformTime::Seconds() - Start) * 1000.0; };
	if (Start >= Deadline) return;
	if (!RetirementContinuation.IsValid())
	{
		if (RetirementQueue.IsEmpty()) return;
		RetirementCursor %= RetirementQueue.Num();
		RetirementCandidate = RetirementQueue[RetirementCursor];
		RetirementCursor = (RetirementCursor + 1) % RetirementQueue.Num();
		if (IsRetirementWanted(RetirementCandidate))
		{
			RemoveRetirement(RetirementCandidate);
			return;
		}
		switch (RetirementCandidate.Representation)
		{
		case 0:
		{
			const auto Bounds = FVoxelFinePageKey{RetirementCandidate.Coordinate, RetirementCandidate.Level}.GetBounds();
			RetirementBounds = {FIntPoint(Bounds.Min.X, Bounds.Min.Y), FIntPoint(Bounds.Max.X, Bounds.Max.Y)};
			break;
		}
		case 4: RetirementBounds = FineCoverageRect(RetirementCandidate.Coordinate); break;
		case 1: RetirementBounds = VoxelProxyCoverageRect({RetirementCandidate.Coordinate, RetirementCandidate.Level}); break;
		case 2: RetirementBounds = SurfaceCoverageRect({FIntPoint(RetirementCandidate.Coordinate.X, RetirementCandidate.Coordinate.Y), RetirementCandidate.Level}); break;
		case 3: RetirementBounds = MacroCoverageRect({FIntPoint(RetirementCandidate.Coordinate.X, RetirementCandidate.Coordinate.Y), RetirementCandidate.Level}); break;
		default: RemoveRetirement(RetirementCandidate); return;
		}
		if (ShouldDeferCoverage(RetirementBounds) || Publisher->IsGroupBusy(RetirementCandidate) ||
			FPlatformTime::Seconds() >= Deadline) return;
		const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, TaskOwnerId, TaskOwnerIncarnation,
			RetirementCandidate.Coordinate, FMath::Max<uint64>(1, AppliedInterestRevision));
		if (!Scheduler.GetTerrainResources()->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::ArrayBytes<FVoxelCoverageRect>(1), RetirementScopeLease)) return;
		RetirementScopeRects.Reserve(1);
		RetirementScopeRects.Add(RetirementBounds);
		RetirementInterestSnapshot = AppliedInterestSnapshot ? AppliedInterestSnapshot : Module.GetCurrentInterestSnapshot();
		RetirementInputRoot = AppliedInputRoot;
		RetirementCoverageRoot = CoverageIndex;
		RetirementHeightfieldRoot = HeightfieldCoverageIndex;
		RetirementContentSnapshot = Module.GetRuntime()->GetChangeHierarchy().CaptureVersionSnapshot();
		RetirementHierarchyResources = Module.GetRuntime()->GetTerrainResources();
		RetirementHierarchyConsumer = Module.GetRuntime()->GetChangeHierarchy().GetControlConsumer();
		const bool bVolume = RetirementCandidate.Representation <= 1 || RetirementCandidate.Representation == 4;
		const int32 Side = 16 << RetirementCandidate.Level;
		RetirementContentMin = FIntVector(RetirementBounds.Min.X, RetirementBounds.Min.Y,
			bVolume ? RetirementCandidate.Coordinate.Z * Side : Module.GetManifest().Settings.MinZ);
		RetirementContentMax = FIntVector(RetirementBounds.Max.X, RetirementBounds.Max.Y,
			bVolume ? RetirementContentMin.Z + Side : Module.GetManifest().Settings.MaxZ);
		RetirementContinuation = CleanupRetiredRepresentationSteps(InNow, RetirementCandidate);
	}
	const FIntVector Min(RetirementBounds.Min.X - 1, RetirementBounds.Min.Y - 1, MIN_int32 + 1);
	const FIntVector Max(RetirementBounds.Max.X + 1, RetirementBounds.Max.Y + 1, MAX_int32);
	for (int32 Step = 0; Step < 4096 && !RetirementContinuation.IsComplete() && FPlatformTime::Seconds() < Deadline; ++Step)
	{
		if (IsRetirementWanted(RetirementCandidate))
		{
			RemoveRetirement(RetirementCandidate);
			ResetRetirementContinuation();
			break;
		}
		if (ShouldDeferCoverage(RetirementBounds) || Publisher->IsGroupBusy(RetirementCandidate) ||
			!CoverageIndex.IsRangeCurrent(RetirementCoverageRoot, Min, Max, true) ||
			!HeightfieldCoverageIndex.IsRangeCurrent(RetirementHeightfieldRoot, Min, Max, true) ||
			!Module.GetRuntime()->GetChangeHierarchy().IsVersionSnapshotCurrent(RetirementContentSnapshot,
				{RetirementContentMin, RetirementContentMax}))
		{
			ResetRetirementContinuation();
			break;
		}
		if (RetirementHierarchyAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked)
		{
			uint64 Version = 0;
			if (!RetirementHierarchyResources ||
				!RetirementHierarchyResources->TryGetReleaseVersionExcludingOwner(RetirementHierarchyConsumer, Version) ||
				Version <= RetirementHierarchyCapacityVersion)
			{
				const EVoxelTerrainRepresentation Representation = RetirementCandidate.Representation == 1 ? EVoxelTerrainRepresentation::VoxelProxy :
					RetirementCandidate.Representation == 2 ? EVoxelTerrainRepresentation::Surface : EVoxelTerrainRepresentation::Macro;
				const FVoxelTerrainSpatialKey Spatial{Representation, RetirementCandidate.Coordinate, RetirementCandidate.Level};
				if (!Module.GetRuntime()->GetChangeHierarchy().CanReleaseGeometryWithoutAllocation(Spatial)) break;
			}
			RetirementHierarchyAdmission = EVoxelTerrainResourceAdmission::Granted;
		}
		if (FPlatformTime::Seconds() >= Deadline) break;
		// 只在当前微步借用局部读取范围，Yield 后立即恢复 coverage pass 的原范围。
		TGuardValue<bool> ReadScope(bRetirementReadScope, true);
		TGuardValue<bool> FullScope(bActiveCoverageFull, false);
		Swap(ActiveDirtyCoverageRects, RetirementScopeRects);
		ON_SCOPE_EXIT { Swap(ActiveDirtyCoverageRects, RetirementScopeRects); };
		RetirementContinuation.Resume();
	}
	if (RetirementContinuation.IsComplete()) ResetRetirementContinuation();
}

bool FVoxelViewManager::ReleaseRetirementGeometry(const FVoxelTerrainSpatialKey& InKey)
{
	const auto* Runtime = Module.GetRuntime();
	if (!Runtime || &Runtime->GetTerrainResources().Get() != RetirementHierarchyResources.Get() ||
		Runtime->GetChangeHierarchy().GetControlConsumer() != RetirementHierarchyConsumer)
	{
		RetirementHierarchyAdmission = EVoxelTerrainResourceAdmission::InvalidRequest;
		return false;
	}
	RetirementHierarchyCapacityVersion = RetirementHierarchyResources->ReleaseVersionExcludingOwner(RetirementHierarchyConsumer);
	FString Error;
	if (Module.GetRuntime()->GetChangeHierarchy().ReleaseGeometry(InKey, Error))
	{
		RetirementHierarchyAdmission = EVoxelTerrainResourceAdmission::Granted;
		return true;
	}
	RetirementHierarchyAdmission = Module.GetRuntime()->GetChangeHierarchy().GetLastResourceAdmission();
	if (RetirementHierarchyAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked)
		UE_LOG(LogTemp, Error, TEXT("Terrain retirement geometry release failed: %s"), *Error);
	return false;
}

FVoxelGameThreadContinuation FVoxelViewManager::CleanupRetiredRepresentationSteps(const double InNow, const FVoxelPublishGroupKey Candidate)
{

	auto CanRetire = [InNow](const double InLastWanted, const bool bReplacementReady, const bool bInsideDomain)
	{
		const double Age = InNow - InLastWanted;
		return Age >= RetireDelaySeconds && (bReplacementReady || (!bInsideDomain && Age >= OutsideDomainRetireDelaySeconds));
	};
	const auto InsideCurrentDomain = [this](auto&& Query)
	{
		TGuardValue<bool> ReadCurrent(bRetirementReadScope, false);
		return Query();
	};

	FCoveragePassSnapshot Snapshot;
	co_await BuildCoverageSnapshotSteps(Snapshot);
	if (!Snapshot.bComplete) co_return;
	if (Candidate.Representation == 0)
	{
		const FVoxelFinePageKey Page{Candidate.Coordinate, Candidate.Level};
		if (GetInputRoot().FinePagePlan.Pages.Contains(Page)) co_return;
		RetireFinePageLayoutState(Page);
		RemoveRetirement(Candidate);
		if (Candidate.Representation <= 3)
		{
			const FVoxelTerrainSpatialKey Spatial{
				static_cast<EVoxelTerrainRepresentation>(Candidate.Representation + 1), Candidate.Coordinate, Candidate.Level};
			GeometryMetrics.Remove(Spatial);
			PresentedGeometryMetrics.Remove(Spatial);
		}
	}
	if (Candidate.Representation == 4)
	{
		const FIntVector Section = Candidate.Coordinate;
		if (GetInputRoot().FineWanted.Contains(Section) || VisibleTerrainNodes.Contains({Section, 0}) || PresentedFineSectionOwners.Contains(Section))
			co_return;
		if (InNow - FineLastWanted.FindRef(Section) < RetireDelaySeconds)
			co_return;
		FineSectionMeshes.Remove(Section);
		FineReady.Remove(Section);
		FineRevisions.Remove(Section);
		FineLastWanted.Remove(Section);
		IndexCoverage(FVoxelViewKey{Section, 0});
		RemoveRetirement(Candidate);
		if (Candidate.Representation <= 3)
		{
			const FVoxelTerrainSpatialKey Spatial{
				static_cast<EVoxelTerrainRepresentation>(Candidate.Representation + 1), Candidate.Coordinate, Candidate.Level};
			GeometryMetrics.Remove(Spatial);
			PresentedGeometryMetrics.Remove(Spatial);
		}
		bReadyTerrainBranchesDirty = true;
	}
	if (Candidate.Representation == 1)
	{
		const FVoxelViewKey Key{Candidate.Coordinate, Candidate.Level};
		if (GetInputRoot().ProxyWanted.Contains(Key) || VisibleTerrainNodes.Contains(Key))
			co_return;
		if (Publisher->IsGroupBusy({1, Key.Coordinate, Key.Level}))
			co_return;
		const double LastWanted = VoxelProxyLastWanted.FindRef(Key);
		bool bReplacement = false;
		co_await IsTerrainVolumeCoveredSteps(Key, Snapshot, true, bReplacement);
		if (!bReplacement)
			co_await VoxelCoverage::IsFullyCovered2DSteps(VoxelProxyCoverageRect(Key), Snapshot.Heightfield, bReplacement);
		if (!CanRetire(LastWanted, bReplacement, InsideCurrentDomain([this, Key] { return IsProxyInsideRenderDomain(Key); })))
		{
			co_return;
		}
		if (!Publisher->Retire(VoxelProxySources.FindRef(Key))) co_return;
		if (!Publisher->Retire(VolumeBoundarySources.FindRef(Key))) co_return;
		while (!ReleaseRetirementGeometry({EVoxelTerrainRepresentation::VoxelProxy, Key.Coordinate, Key.Level}))
		{
			if (RetirementHierarchyAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked) co_return;
			co_await FVoxelGameThreadContinuation::Yield();
		}
		if (FVoxelViewSourceId SourceId = VoxelProxySources.FindRef(Key))
		{
			Publisher->Forget(SourceId);
		}
		VoxelProxySources.Remove(Key);
		bReadyTerrainBranchesDirty |= VoxelProxyReady.Remove(Key) > 0;
		VoxelProxyData.Remove(Key);
		AirVoxelProxyNodes.Remove(Key);
		ProxySurfaceCoverage.Remove(Key);
		VoxelProxyRevisions.Remove(Key);
		VolumeTransitionSignatures.Remove(Key);
		PreparedVolumeSignatures.Remove(Key);
		PreparedVolumeMeshes.Remove(Key);
		VoxelProxyLastWanted.Remove(Key);
		PreparedVolumeBoundaryMeshes.Remove(Key);
		if (FVoxelViewSourceId Boundary = VolumeBoundarySources.FindRef(Key))
		{
			Publisher->Forget(Boundary);
		}
		VolumeBoundarySources.Remove(Key);
		IndexCoverage(Key);
		RemoveRetirement(Candidate);
		if (Candidate.Representation <= 3)
		{
			const FVoxelTerrainSpatialKey Spatial{
				static_cast<EVoxelTerrainRepresentation>(Candidate.Representation + 1), Candidate.Coordinate, Candidate.Level};
			GeometryMetrics.Remove(Spatial);
			PresentedGeometryMetrics.Remove(Spatial);
		}
	}
	if (Candidate.Representation == 2)
	{
		const FVoxelSurfaceTileKey Key{FIntPoint(Candidate.Coordinate.X, Candidate.Coordinate.Y), Candidate.Level};
		if (GetInputRoot().SurfaceWanted.Contains(Key) || InNow - SurfaceLastWanted.FindRef(Key) < RetireDelaySeconds)
			co_return;
		if (Publisher->IsGroupBusy({2, Key.Coordinate, Key.Level}))
			co_return;
		if (VoxelCoverage::IntersectsAny2D(SurfaceCoverageRect(Key), PendingHeightfieldHandoffRects))
			co_return;
		bool bReplacement = false;
		co_await VoxelCoverage::IsFullyCovered2DSteps(SurfaceCoverageRect(Key), Snapshot.Macro, bReplacement);
		if (!bReplacement)
			co_await VoxelCoverage::IsFullyCovered2DSteps(SurfaceCoverageRect(Key), Snapshot.ProxySurface, bReplacement);
		if (GetInputRoot().SurfaceWanted.Contains(Key) || !CanRetire(SurfaceLastWanted.FindRef(Key), bReplacement,
			InsideCurrentDomain([this, Key] { return IsSurfaceInsideRenderDomain(Key); })))
			co_return;
		if (!Publisher->Retire(SurfaceSources.FindRef(Key))) co_return;
		if (!Publisher->Retire(WaterSources.FindRef(Key))) co_return;
		while (!ReleaseRetirementGeometry({EVoxelTerrainRepresentation::Surface, FIntVector(Key.Coordinate.X, Key.Coordinate.Y, 0), Key.Level}))
		{
			if (RetirementHierarchyAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked) co_return;
			co_await FVoxelGameThreadContinuation::Yield();
		}
		if (FVoxelViewSourceId SourceId = SurfaceSources.FindRef(Key))
		{
			Publisher->Forget(SourceId);
		}
		if (FVoxelViewSourceId Water = WaterSources.FindRef(Key))
		{
			Publisher->Forget(Water);
		}
		SurfaceSources.Remove(Key);
		WaterSources.Remove(Key);
		bHeightfieldTopologyDirty |= SurfaceReady.Remove(Key) > 0;
		SurfaceData.Remove(Key);
		WaterData.Remove(Key);
		SurfaceRevisions.Remove(Key);
		SurfaceLastWanted.Remove(Key);
		IndexCoverage(Key);
		RemoveRetirement(Candidate);
		if (Candidate.Representation <= 3)
		{
			const FVoxelTerrainSpatialKey Spatial{
				static_cast<EVoxelTerrainRepresentation>(Candidate.Representation + 1), Candidate.Coordinate, Candidate.Level};
			GeometryMetrics.Remove(Spatial);
			PresentedGeometryMetrics.Remove(Spatial);
		}
		MarkCoverageDirty(SurfaceCoverageRect(Key));
	}
	if (Candidate.Representation == 3)
	{
		const FVoxelMacroTileKey Key{FIntPoint(Candidate.Coordinate.X, Candidate.Coordinate.Y), Candidate.Level};
		if (GetInputRoot().MacroWanted.Contains(Key) || InNow - MacroLastWanted.FindRef(Key) < RetireDelaySeconds)
			co_return;
		if (Publisher->IsGroupBusy({3, Key.Coordinate, Key.Level}))
			co_return;
		if (VoxelCoverage::IntersectsAny2D(MacroCoverageRect(Key), PendingHeightfieldHandoffRects))
			co_return;
		bool bReplacement = false;
		co_await VoxelCoverage::IsFullyCovered2DSteps(MacroCoverageRect(Key), Snapshot.Surface, bReplacement);
		if (GetInputRoot().MacroWanted.Contains(Key) || !CanRetire(MacroLastWanted.FindRef(Key), bReplacement,
			InsideCurrentDomain([this, Key] { return IsMacroInsideRenderDomain(Key); })))
			co_return;
		if (!Publisher->Retire(MacroSources.FindRef(Key))) co_return;
		while (!ReleaseRetirementGeometry({EVoxelTerrainRepresentation::Macro, FIntVector(Key.Coordinate.X, Key.Coordinate.Y, 0), Key.Level}))
		{
			if (RetirementHierarchyAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked) co_return;
			co_await FVoxelGameThreadContinuation::Yield();
		}
		if (FVoxelViewSourceId SourceId = MacroSources.FindRef(Key))
		{
			Publisher->Forget(SourceId);
		}
		MacroSources.Remove(Key);
		bHeightfieldTopologyDirty |= MacroReady.Remove(Key) > 0;
		MacroData.Remove(Key);
		MacroRevisions.Remove(Key);
		MacroLastWanted.Remove(Key);
		IndexCoverage(Key);
		RemoveRetirement(Candidate);
		if (Candidate.Representation <= 3)
		{
			const FVoxelTerrainSpatialKey Spatial{
				static_cast<EVoxelTerrainRepresentation>(Candidate.Representation + 1), Candidate.Coordinate, Candidate.Level};
			GeometryMetrics.Remove(Spatial);
			PresentedGeometryMetrics.Remove(Spatial);
		}
		MarkCoverageDirty(MacroCoverageRect(Key));
	}
}
