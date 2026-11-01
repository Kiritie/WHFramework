#include "Voxel/Rendering/VoxelViewManager.h"

#include "Misc/ScopeExit.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Rendering/VoxelMeshClipper.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/VoxelModule.h"

void FVoxelViewManager::AdvanceInitialLayout(const double InDeadline)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewInitialLayout);
	if (FPlatformTime::Seconds() >= InDeadline) return;
	if (!InitialLayoutContinuation.IsValid())
	{
		if (!AreTransitionDirectoriesReady()) return;
		bInitialLayoutDirty = false;
		bActiveCoverageFull = true;
		ActiveDirtyCoverageRects.Reset();
		CoverageInterestSnapshot = AppliedInterestSnapshot;
		CoverageInputRoot = AppliedInputRoot;
		ActiveCoveragePublishGroups = MoveTemp(OpenPublishGroups);
		InitialLayoutContinuation = ResolveInitialLayoutSteps();
	}
	{
		TGuardValue<bool> ReadScope(bCoverageReadScope, true);
		while (!InitialLayoutContinuation.IsComplete() && FPlatformTime::Seconds() < InDeadline)
			InitialLayoutContinuation.Resume();
	}
	if (!InitialLayoutContinuation.IsComplete()) return;
	InitialLayoutContinuation.Reset();
	CoverageInterestSnapshot.Reset();
	ReleaseInputRoot(CoverageInputRoot);
	for (const auto& Rect : FailedPublishCoverageRects) MarkCoverageDirty(Rect);
	FailedPublishCoverageRects.Reset();
}

bool FVoxelViewManager::NeedsInitialPublication(const FVoxelPublishGroupKey& InKey, const FVoxelViewSourceId InSource, const bool bInVisible) const
{
	if (ActiveCoveragePublishGroups.Contains(InKey)) return true;
	return InSource && !Publisher->IsGroupBusy(InKey) &&
		(!Publisher->IsCommitted(InSource) || Publisher->IsHidden(InSource) == bInVisible);
}

FVoxelGameThreadContinuation FVoxelViewManager::PublishInitialSourcesSteps(FPublicationLayout& InLayout, FPublicationSources& InSources)
{
	check(GetInputRoot().bInitialLayout);
	const auto Column = [](const FVoxelCoverageRect& Bounds)
	{
		return FBox(FVector(Bounds.Min.X, Bounds.Min.Y, -1.e12), FVector(Bounds.Max.X, Bounds.Max.Y, 1.e12));
	};
	TerrainCandidateCount = InLayout.TerrainSourceCandidates.Num();
	for (const auto& Key : InLayout.TerrainSourceCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto Source = VoxelProxySources.FindRef(Key);
		const FVoxelPublishGroupKey GroupKey{1, Key.Coordinate, Key.Level};
		const bool bVisible = InLayout.TargetTerrainNodes.Contains(Key);
		if (!Source || !NeedsInitialPublication(GroupKey, Source, bVisible)) continue;
		const auto Group = BeginPublishGroup(GroupKey);
		if (bVisible)
		{
			TArray<FBox> Exclusions = *InSources.FineBoxes;
			for (const auto& Other : GetInputRoot().ProxyWanted)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				if (Other.Level > 0 && Other.Level < Key.Level)
				{
					const auto Bounds = VoxelProxyCoverageBox(Other);
					Exclusions.Add(FBox(FVector(Bounds.Min), FVector(Bounds.Max)));
				}
			}
			co_await UpdatePublicationCoverageSteps(InSources, Group, Source, MoveTemp(Exclusions));
		}
		Publisher->SetHidden(Group, Source, !bVisible);
		if (const auto Boundary = VolumeBoundarySources.FindRef(Key)) Publisher->SetHidden(Group, Boundary, !bVisible);
	}
	for (const auto& Key : InLayout.LocalSurfaceKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto Source = SurfaceSources.FindRef(Key);
		const auto Water = WaterSources.FindRef(Key);
		const FVoxelPublishGroupKey GroupKey{2, Key.Coordinate, Key.Level};
		const bool bBlocked = InSources.BlockedHeightfieldOwners.Contains({EVoxelHeightfieldRepresentation::Surface, Key.Coordinate, Key.Level});
		const bool bVisible = !bBlocked || (Publisher->HasPresentation(Source) && !Publisher->IsHidden(Source));
		if (!Source || (!NeedsInitialPublication(GroupKey, Source, bVisible) &&
			(!Water || !NeedsInitialPublication(GroupKey, Water, bVisible)))) continue;
		const auto Group = BeginPublishGroup(GroupKey);
		if (bBlocked)
		{
			if (Publisher->HasPresentation(Source))
				co_await UpdatePublicationCoverageSteps(InSources, Group, Source, {}, InSources.HeightfieldBoxes, true);
			else Publisher->SetHidden(Group, Source, true);
			if (Water) co_await UpdatePublicationCoverageSteps(InSources, Group, Water, {}, InSources.HeightfieldBoxes, true);
			continue;
		}
		const auto Rect = SurfaceCoverageRect(Key);
		TArray<FBox> Exclusions;
		for (const auto& Other : GetInputRoot().SurfaceWanted)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (Other.Level < Key.Level && Rect.Intersects(SurfaceCoverageRect(Other))) Exclusions.Add(Column(SurfaceCoverageRect(Other)));
		}
		if (!Publisher->IsCommitted(Source)) InSources.RecheckCoverageRects.Add(Rect);
		co_await UpdatePublicationCoverageSteps(InSources, Group, Source, Exclusions, InSources.HeightfieldBoxes);
		if (Water) co_await UpdatePublicationCoverageSteps(InSources, Group, Water, MoveTemp(Exclusions), InSources.HeightfieldBoxes);
		Publisher->SetHidden(Group, Source, false);
		if (Water) Publisher->SetHidden(Group, Water, false);
	}
	for (const auto& Key : InLayout.LocalMacroKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto Source = MacroSources.FindRef(Key);
		const FVoxelPublishGroupKey GroupKey{3, Key.Coordinate, Key.Level};
		const bool bBlocked = InSources.BlockedHeightfieldOwners.Contains({EVoxelHeightfieldRepresentation::Macro, Key.Coordinate, Key.Level});
		const bool bVisible = !bBlocked || (Publisher->HasPresentation(Source) && !Publisher->IsHidden(Source));
		if (!Source || !NeedsInitialPublication(GroupKey, Source, bVisible)) continue;
		const auto Group = BeginPublishGroup(GroupKey);
		if (bBlocked)
		{
			if (Publisher->HasPresentation(Source))
				co_await UpdatePublicationCoverageSteps(InSources, Group, Source, {}, InSources.HeightfieldBoxes, true);
			else Publisher->SetHidden(Group, Source, true);
			continue;
		}
		const auto Rect = MacroCoverageRect(Key);
		TArray<FBox> Exclusions;
		for (const auto& Other : GetInputRoot().SurfaceWanted)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (Rect.Intersects(SurfaceCoverageRect(Other))) Exclusions.Add(Column(SurfaceCoverageRect(Other)));
		}
		for (const auto& Other : GetInputRoot().MacroWanted)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (Other.Level < Key.Level && Rect.Intersects(MacroCoverageRect(Other))) Exclusions.Add(Column(MacroCoverageRect(Other)));
		}
		if (!Publisher->IsCommitted(Source)) InSources.RecheckCoverageRects.Add(Rect);
		co_await UpdatePublicationCoverageSteps(InSources, Group, Source, MoveTemp(Exclusions), InSources.HeightfieldBoxes);
		Publisher->SetHidden(Group, Source, false);
	}
	for (const auto& Key : InSources.HeightfieldOwners)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto Source = HeightfieldTransitionSources.FindRef(Key);
		const auto Rect = Key.Representation == EVoxelHeightfieldRepresentation::Surface ?
			SurfaceCoverageRect({Key.Coordinate, Key.Level}) : MacroCoverageRect({Key.Coordinate, Key.Level});
		const bool bBlocked = InSources.BlockedHeightfieldOwners.Contains(Key) || VoxelCoverage::IntersectsAny2D(Rect, PendingHeightfieldHandoffRects);
		if (bBlocked) InSources.HeightfieldToCommit.Remove(Key);
		const bool bRetain = bBlocked && Publisher->HasPresentation(Source);
		const bool bVisible = bRetain ? !Publisher->IsHidden(Source) : DesiredTransitionSignatures.Contains(Key) && !bBlocked;
		const FVoxelPublishGroupKey GroupKey{static_cast<uint8>(Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 4 : 5), Key.Coordinate, Key.Level};
		if (!Source || !NeedsInitialPublication(GroupKey, Source, bVisible)) continue;
		const auto Group = BeginPublishGroup(GroupKey);
		co_await UpdatePublicationCoverageSteps(InSources, Group, Source, {}, InSources.TransitionBoxes, bRetain);
		if (!bRetain) Publisher->SetHidden(Group, Source, !bVisible);
	}
}

FVoxelGameThreadContinuation FVoxelViewManager::ResolveInitialLayoutSteps()
{
	check(GetInputRoot().bInitialLayout);
	bCoverageRetryRequested = false;
	ON_SCOPE_EXIT
	{
		if (bCoverageRetryRequested)
		{
			// 失败发生在提交之前；归还原分组，下一轮按最新输入重新计算覆盖。
			RestorePublicationGroups();
			bHeightfieldTopologyDirty = true;
		}
	};
	co_await PrepareInitialLayoutMasksSteps();
	if (bCoverageRetryRequested) co_return;
	FPublicationLayout Layout;
	Layout.PlanningStartedSeconds = FPlatformTime::Seconds();
	bool bCaptured = false;
	co_await CaptureCoverageCandidateSteps(Layout.LocalFineKeys, Layout.LocalProxyKeys,
		Layout.LocalSurfaceKeys, Layout.LocalMacroKeys, Layout.CandidateLease, bCaptured);
	if (!bCaptured) { RetryActiveCoverage(); co_return; }
	for (const auto& Key : Layout.LocalProxyKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (VisibleTerrainNodes.Contains(Key)) Layout.PreviousTerrainNodes.Add(Key);
		if (!VoxelProxyReady.Contains(Key)) continue;
		Layout.TargetTerrainNodes.Add(Key);
		Layout.TerrainSourceCandidates.Add(Key);
	}
	co_await RebuildVolumeTransitionSteps(GetInputRoot().ProxyWanted);
	if (bCoverageRetryRequested) co_return;
	co_await PublishTerrainLayoutSteps(MoveTemp(Layout));
}

FVoxelGameThreadContinuation FVoxelViewManager::PrepareInitialLayoutMasksSteps()
{
	auto& Root = GetInputRoot();
	if (Root.InitialFineExclusions) co_return;
	const int64 FineCount = int64(Root.FineWanted.Num()) + GetViewInterest().FineShardDemands.Num();
	const int64 TotalCount = FineCount + Root.ProxyWanted.Num();
	if (TotalCount > MAX_int32) { RetryActiveCoverage(); co_return; }
	const uint64 Bytes = 3 * VoxelBoundedAllocation::SharedBytes<FLayoutExclusions>() +
		VoxelBoundedAllocation::ArrayBytes<FBox>(int32(FineCount)) +
		2 * VoxelBoundedAllocation::ArrayBytes<FBox>(int32(TotalCount));
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, TaskOwnerId,
		TaskOwnerIncarnation, {}, AppliedInterestRevision);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (!Scheduler.GetTerrainResources()->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, Lease))
	{
		RetryActiveCoverage();
		co_return;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
	bool bScratchGranted = false;
	co_await AcquireCoverageCapacitySteps(VoxelBoundedAllocation::ArrayBytes<FBox>(int32(TotalCount)), Scratch, bScratchGranted);
	if (!bScratchGranted) { RetryActiveCoverage(); co_return; }
	const auto Fine = MakeShared<FLayoutExclusions, ESPMode::ThreadSafe>();
	const auto Heightfield = MakeShared<FLayoutExclusions, ESPMode::ThreadSafe>();
	const auto Transition = MakeShared<FLayoutExclusions, ESPMode::ThreadSafe>();
	Fine->ResourceLease = Heightfield->ResourceLease = Transition->ResourceLease = Lease;
	Fine->Boxes.Reserve(int32(FineCount));
	Heightfield->Boxes.Reserve(int32(TotalCount));
	Transition->Boxes.Reserve(int32(TotalCount));
	for (const auto& Key : Root.FineWanted)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto Bounds = FineCoverageBox(Key);
		Fine->Boxes.Add(FBox(FVector(Bounds.Min), FVector(Bounds.Max)));
	}
	for (const auto& Pair : GetViewInterest().FineShardDemands)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		Fine->Boxes.Add(FBox(FVector(Pair.Key.Min), FVector(Pair.Key.Min + FIntVector(Pair.Key.SideCells))));
	}
	co_await VoxelMeshClipper::NormalizeBoxesSteps(Fine->Boxes);
	co_await VoxelGameThread::AppendSteps(Heightfield->Boxes, Fine->Boxes);
	for (const auto& Key : Root.ProxyWanted)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto Bounds = VoxelProxyCoverageBox(Key);
		Heightfield->Boxes.Add(FBox(FVector(Bounds.Min), FVector(Bounds.Max)));
	}
	co_await VoxelMeshClipper::NormalizeBoxesSteps(Heightfield->Boxes);
	for (FBox Bounds : Heightfield->Boxes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		Bounds.Min.Z = -1.e12;
		Bounds.Max.Z = 1.e12;
		Transition->Boxes.Add(Bounds);
	}
	co_await VoxelMeshClipper::NormalizeBoxesSteps(Transition->Boxes);
	Root.InitialFineExclusions = TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe>(Fine, &Fine->Boxes);
	Root.InitialHeightfieldExclusions = TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe>(Heightfield, &Heightfield->Boxes);
	Root.InitialTransitionExclusions = TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe>(Transition, &Transition->Boxes);
}
