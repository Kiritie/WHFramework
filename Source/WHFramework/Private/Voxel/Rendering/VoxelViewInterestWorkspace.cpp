#include "Voxel/Rendering/VoxelViewManager.h"

#include "Async/Async.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/VoxelModule.h"

namespace
{
	template<typename PayloadType>
	struct TVoxelInterestReclaimTask
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> TaskLease;
		TSharedPtr<PayloadType, ESPMode::ThreadSafe> Payload;
		void operator()() const
		{
			// 最后一个工作区/不可变根引用只在 Worker 销毁；任务内存也由同一预授予保活。
		}
	};
}

void FVoxelViewManager::ReleaseInputRoot(TSharedPtr<FInterestInputRoot, ESPMode::ThreadSafe>& InRoot)
{
	if (!InRoot) return;
	if (!InRoot.IsUnique()) { InRoot.Reset(); return; }
	TVoxelInterestReclaimTask<FInterestInputRoot> Reclaim{InRoot->ResourceLease, MoveTemp(InRoot)};
	UE::Tasks::Launch(UE_SOURCE_LOCATION, MoveTemp(Reclaim));
}

void FVoxelViewManager::ReleaseInterestWorkspace(TSharedPtr<FInterestWorkspace, ESPMode::ThreadSafe>& InWorkspace)
{
	if (!InWorkspace) return;
	TVoxelInterestReclaimTask<FInterestWorkspace> Reclaim{InWorkspace->MetadataLease, MoveTemp(InWorkspace)};
	UE::Tasks::Launch(UE_SOURCE_LOCATION, MoveTemp(Reclaim));
}

FVoxelViewManager::FInterestInputRoot& FVoxelViewManager::GetInputRoot()
{
	return const_cast<FInterestInputRoot&>(static_cast<const FVoxelViewManager&>(*this).GetInputRoot());
}

const FVoxelViewManager::FInterestInputRoot& FVoxelViewManager::GetInputRoot() const
{
	if (bRetirementReadScope && RetirementInputRoot) return *RetirementInputRoot;
	if (bCoverageReadScope && CoverageInputRoot) return *CoverageInputRoot;
	return AppliedInputRoot ? *AppliedInputRoot : EmptyInputRoot;
}

bool FVoxelViewManager::BeginInterestWorkspace(TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> InSnapshot)
{
	if (InterestWorkspace || CommittedWorkspaceEvents || !InSnapshot || !InSnapshot->PublicationInputs ||
		InSnapshot->PublicationRevision == 0 || !InSnapshot->FinePagePlan || !InSnapshot->FineSections)
	{
		return false;
	}
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, TaskOwnerId,
		TaskOwnerIncarnation, {}, InSnapshot->PublicationRevision);
	const auto& Inputs = *InSnapshot->PublicationInputs;
	const auto& Current = GetInputRoot();
	const int64 AddedCount64 = int64(Inputs.Fine.Num()) + Inputs.Proxy.Num() + Inputs.Surface.Num() + Inputs.Macro.Num();
	const int64 RemovedCount64 = int64(Current.FineWanted.Num()) + Current.ProxyWanted.Num() +
		Current.SurfaceWanted.Num() + Current.MacroWanted.Num() + Current.FinePagePlan.Pages.Num();
	const int64 ShardsCount64 = int64(FineShardSlots.Num()) + Current.FineWanted.Num() +
		(AppliedInterestSnapshot ? AppliedInterestSnapshot->FineShardDemands.Num() : 0);
	if (AddedCount64 > MAX_int32 || RemovedCount64 > MAX_int32 || ShardsCount64 > MAX_int32 ||
		Inputs.Roots.Num() > MAX_int32 / (VoxelStreamingGeometry::MaximumSpatialLevel + 1)) return false;
	const int32 AddedCount = int32(AddedCount64), RemovedCount = int32(RemovedCount64), ShardsCount = int32(ShardsCount64);
	// 每个潜在实际 Atom/旧事实及新 Wanted 都有失败键槽；容量不足时保留旧 Current，不能先接管后丢事件。
	const int64 RetryCount = AddedCount64 + CoverageIndex.NumRecords() + FineReady.Num() + VoxelProxyReady.Num() +
		SurfaceReady.Num() + MacroReady.Num() + FineShardOwnershipAtomCapacity + Inputs.MaximumFeedbackSections;
	if (RetryCount > MAX_int32 || !ReserveCoverageIndexRetries(int32(RetryCount))) return false;
	const uint64 Bytes = VoxelBoundedAllocation::SharedBytes<FInterestWorkspace>() +
		VoxelBoundedAllocation::SharedBytes<FInterestInputRoot>() +
		VoxelBoundedAllocation::TaskBytes<TVoxelInterestReclaimTask<FInterestWorkspace>>() +
		VoxelBoundedAllocation::TaskBytes<TVoxelInterestReclaimTask<FInterestInputRoot>>() +
		VoxelBoundedAllocation::SetBytes<FIntVector>(Inputs.Fine.Num()) +
		VoxelBoundedAllocation::SetBytes<FVoxelViewKey>(Inputs.Proxy.Num()) +
		VoxelBoundedAllocation::SetBytes<FVoxelViewKey>(Inputs.Roots.Num()) +
		VoxelBoundedAllocation::SetBytes<FVoxelSurfaceTileKey>(Inputs.Surface.Num()) +
		VoxelBoundedAllocation::SetBytes<FVoxelMacroTileKey>(Inputs.Macro.Num()) +
		VoxelBoundedAllocation::SetBytes<FVoxelFinePageKey>(Inputs.FinePages.Num()) +
		VoxelBoundedAllocation::MapBytes<FIntVector, FVoxelFinePageKey>(Inputs.FineOwners.Num()) +
		FVoxelCoverageIndex::MaximumStorageBytes(Inputs.Roots.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelPublishGroupKey>(AddedCount) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelPublishGroupKey>(RemovedCount) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelFinePageKey>(Inputs.FinePages.Num()) +
		2 * VoxelBoundedAllocation::ArrayBytes<FVoxelFineShardKey>(ShardsCount);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (!Scheduler.GetTerrainResources()->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
		Bytes, Lease)) return false;
	InterestWorkspace = MakeShared<FInterestWorkspace, ESPMode::ThreadSafe>();
	InterestWorkspace->MetadataLease = Lease;
	InterestWorkspace->Root = MakeShared<FInterestInputRoot, ESPMode::ThreadSafe>();
	InterestWorkspace->Root->ResourceLease = MoveTemp(Lease);
	auto& Root = *InterestWorkspace->Root;
	Root.FineWanted.Reserve(Inputs.Fine.Num());
	Root.ProxyWanted.Reserve(Inputs.Proxy.Num());
	Root.IndexedTerrainRoots.Reserve(Inputs.Roots.Num());
	Root.SurfaceWanted.Reserve(Inputs.Surface.Num());
	Root.MacroWanted.Reserve(Inputs.Macro.Num());
	Root.FinePagePlan.Pages.Reserve(Inputs.FinePages.Num());
	Root.FinePagePlan.SectionOwners.Reserve(Inputs.FineOwners.Num());
	Root.TerrainRootIndex.ReserveForRecords(Inputs.Roots.Num());
	Root.TerrainRootIndex.BindStorageLease(Root.ResourceLease);
	InterestWorkspace->ChangedCoverage.Reserve(AddedCount);
	InterestWorkspace->Retirements.Reserve(RemovedCount);
	InterestWorkspace->LayoutChanges.Reserve(Inputs.FinePages.Num());
	InterestWorkspace->RetiredShards.Reserve(ShardsCount);
	InterestWorkspace->ChangedShardOwners.Reserve(ShardsCount);
	InterestWorkspace->Snapshot = MoveTemp(InSnapshot);
	PendingInterestSnapshot = InterestWorkspace->Snapshot;
	PendingPublicationInputs = PendingInterestSnapshot->PublicationInputs;
	PendingInterestRevision = PendingInterestSnapshot->PublicationRevision;
	InterestInputPhase = InterestInputCursor = 0;
	return true;
}

void FVoxelViewManager::AdvanceInterestWorkspace(const double InDeadline)
{
	if (!InterestWorkspace) return;
	auto& Workspace = *InterestWorkspace;
	const auto& Inputs = *Workspace.Snapshot->PublicationInputs;
	const auto NextPhase = [&Workspace] { ++Workspace.Phase; Workspace.Cursor = 0; };
	const auto AddOne = [&Workspace, &NextPhase](const auto& Keys, auto&& Operation)
	{
		if (!Keys.IsValidIndex(Workspace.Cursor)) { NextPhase(); return; }
		Operation(Keys[Workspace.Cursor++]);
	};
	const auto VisitOne = [&Workspace, &NextPhase](const auto& Set, auto&& Operation)
	{
		if (Workspace.Cursor >= Set.GetMaxIndex()) { NextPhase(); return; }
		const auto Id = FSetElementId::FromInteger(Workspace.Cursor++);
		if (Set.IsValidId(Id)) Operation(Set[Id]);
	};
	for (int32 Work = 0; Work < 8192 && Workspace.Phase < 14 && FPlatformTime::Seconds() < InDeadline; ++Work)
	{
		const double Start = FPlatformTime::Seconds();
		if (!TerrainFrameBudget.TryStart(EVoxelPublishPhase::Plan, Start, InterestMicrostepEstimateMilliseconds)) return;
		switch (Workspace.Phase)
		{
		case 0:
			AddOne(Inputs.Roots, [&Workspace](const FVoxelViewKey& Key)
			{
				Workspace.Root->IndexedTerrainRoots.Add(Key);
				Workspace.Root->MaximumTerrainRootLevel = FMath::Max(Workspace.Root->MaximumTerrainRootLevel, Key.Level);
				auto Record = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
				Record->Geometry.Key = {EVoxelTerrainRepresentation::VoxelProxy, Key.Coordinate, Key.Level};
				Record->Geometry.SampleStepCells = 2;
				const auto Bounds = Key.GetBounds();
				Record->Min = Bounds.Min;
				Record->Max = Bounds.Max;
				FString Error;
				verify(Workspace.Root->TerrainRootIndex.Upsert(Record, Error));
			});
			break;
		case 1:
			AddOne(Inputs.Fine, [this, &Workspace](const FIntVector& Key)
			{
				Workspace.Root->FineWanted.Add(Key);
				if (!GetInputRoot().FineWanted.Contains(Key)) Workspace.ChangedCoverage.Add({4, Key, 0});
			});
			break;
		case 2:
			AddOne(Inputs.Proxy, [this, &Workspace](const FVoxelViewKey& Key)
			{
				Workspace.Root->ProxyWanted.Add(Key);
				if (!GetInputRoot().ProxyWanted.Contains(Key)) Workspace.ChangedCoverage.Add({1, Key.Coordinate, Key.Level});
			});
			break;
		case 3:
			AddOne(Inputs.Surface, [this, &Workspace](const FVoxelSurfaceTileKey& Key)
			{
				Workspace.Root->SurfaceWanted.Add(Key);
				if (!GetInputRoot().SurfaceWanted.Contains(Key)) Workspace.ChangedCoverage.Add({2, Key.Coordinate, Key.Level});
			});
			break;
		case 4:
			AddOne(Inputs.Macro, [this, &Workspace](const FVoxelMacroTileKey& Key)
			{
				Workspace.Root->MacroWanted.Add(Key);
				if (!GetInputRoot().MacroWanted.Contains(Key)) Workspace.ChangedCoverage.Add({3, Key.Coordinate, Key.Level});
			});
			break;
		case 5:
			AddOne(Inputs.FinePages, [this, &Workspace](const FVoxelFinePageKey& Page)
			{
				Workspace.Root->FinePagePlan.Pages.Add(Page);
				if (!GetInputRoot().FinePagePlan.Pages.Contains(Page)) Workspace.LayoutChanges.Add(Page);
			});
			break;
		case 6:
			AddOne(Inputs.FineOwners, [&Workspace](const auto& Pair)
			{
				Workspace.Root->FinePagePlan.SectionOwners.Add(Pair.Key, Pair.Value);
			});
			break;
		case 7:
			VisitOne(GetInputRoot().FineWanted, [&Workspace](const FIntVector& Key)
			{
				if (!Workspace.Root->FineWanted.Contains(Key)) Workspace.Retirements.Add({4, Key, 0});
			});
			break;
		case 8:
			VisitOne(GetInputRoot().ProxyWanted, [&Workspace](const FVoxelViewKey& Key)
			{
				if (!Workspace.Root->ProxyWanted.Contains(Key)) Workspace.Retirements.Add({1, Key.Coordinate, Key.Level});
			});
			break;
		case 9:
			VisitOne(GetInputRoot().SurfaceWanted, [&Workspace](const FVoxelSurfaceTileKey& Key)
			{
				if (!Workspace.Root->SurfaceWanted.Contains(Key)) Workspace.Retirements.Add({2, Key.Coordinate, Key.Level});
			});
			break;
		case 10:
			VisitOne(GetInputRoot().MacroWanted, [&Workspace](const FVoxelMacroTileKey& Key)
			{
				if (!Workspace.Root->MacroWanted.Contains(Key)) Workspace.Retirements.Add({3, Key.Coordinate, Key.Level});
			});
			break;
		case 11:
			VisitOne(GetInputRoot().FinePagePlan.Pages, [&Workspace](const FVoxelFinePageKey& Page)
			{
				if (!Workspace.Root->FinePagePlan.Pages.Contains(Page)) Workspace.Retirements.Add({0, Page.Coordinate, Page.Level});
			});
			break;
		case 12:
			if (Workspace.Cursor >= FineShardSlotOrder.GetMaxIndex()) { NextPhase(); break; }
			{
				const int32 Index = Workspace.Cursor++;
				if (!FineShardSlotOrder.IsValidIndex(Index)) break;
				const auto& Key = FineShardSlotOrder[Index];
				if (Key.SideCells == 16)
				{
					if (!Workspace.Root->FineWanted.Contains(Key.Min / 16)) Workspace.RetiredShards.Add(Key);
				}
				else if (!Workspace.Snapshot->FineShardDemands.Contains(Key)) Workspace.RetiredShards.Add(Key);
				else if (AppliedInterestSnapshot && AppliedInterestSnapshot->FineShardOwners.FindRef(Key) != Workspace.Snapshot->FineShardOwners.FindRef(Key))
					Workspace.ChangedShardOwners.Add(Key);
			}
			break;
		case 13:
			NextPhase();
			break;
		}
		const double End = FPlatformTime::Seconds();
		InterestMicrostepEstimateMilliseconds = FMath::Lerp(InterestMicrostepEstimateMilliseconds, FMath::Max(0.001, (End - Start) * 1000.0), 0.15);
		TerrainFrameBudget.Finish(EVoxelPublishPhase::Plan, Start, End);
		InterestInputPhase = Workspace.Phase;
		InterestInputCursor = Workspace.Cursor;
	}
}

bool FVoxelViewManager::TryCommitInterestRoot()
{
	if (!InterestWorkspace || InterestWorkspace->Phase != 14 || CommittedWorkspaceEvents) return false;
	const auto Latest = InterestWorkspace->Snapshot;
	if (!Latest || !Module.CanCommitTerrainInterest(*Latest))
	{
		if (Latest) Module.AbandonTerrainInterest(*Latest);
		CancelInterestWorkspace();
		return false;
	}
	const double Start = FPlatformTime::Seconds();
	if (!TerrainFrameBudget.TryStart(EVoxelPublishPhase::Plan, Start, InterestMicrostepEstimateMilliseconds)) return false;
	// 同一次GT步骤交换全部Wanted、根索引、布局与查询快照。
	auto Previous = MoveTemp(AppliedInputRoot);
	AppliedInputRoot = MoveTemp(InterestWorkspace->Root);
	AppliedInterestSnapshot = Latest;
	AppliedInterestRevision = Latest->PublicationRevision;
	Admissions = Latest->Admissions;
	AdmissionPriorities = Latest->AdmissionPriorities;
	FinePageLayoutRevision = Latest->LayoutRevision;
	InterestWorkspace->Snapshot = Latest;
	InterestWorkspace->Phase = InterestWorkspace->Cursor = 0;
	CommittedWorkspaceEvents = MoveTemp(InterestWorkspace);
	PendingInterestSnapshot.Reset();
	PendingPublicationInputs.Reset();
	PendingInterestRevision = 0;
	bTaskPrioritiesDirty = true;
	bTerrainBuildStageDirty = true;
	Publisher->InvalidatePriorities();
	Module.CommitTerrainInterest(Latest);
	ReleaseInputRoot(Previous);
	TerrainFrameBudget.Finish(EVoxelPublishPhase::Plan, Start, FPlatformTime::Seconds());
	return true;
}

void FVoxelViewManager::CancelInterestWorkspace()
{
	ReleaseInterestWorkspace(InterestWorkspace);
	PendingInterestSnapshot.Reset();
	PendingPublicationInputs.Reset();
	PendingInterestRevision = 0;
}

void FVoxelViewManager::AdvanceCommittedInterestEvents(const double InDeadline)
{
	if (!CommittedWorkspaceEvents) return;
	auto& Events = *CommittedWorkspaceEvents;
	const auto Next = [&Events] { ++Events.Phase; Events.Cursor = 0; };
	const auto Visit = [&Events, &Next](const auto& Values, auto&& Operation)
	{
		if (!Values.IsValidIndex(Events.Cursor)) { Next(); return; }
		Operation(Values[Events.Cursor++]);
	};
	for (int32 Work = 0; Work < 8192 && FPlatformTime::Seconds() < InDeadline; ++Work)
	{
		const double Start = FPlatformTime::Seconds();
		if (!TerrainFrameBudget.TryStart(EVoxelPublishPhase::Plan, Start, InterestMicrostepEstimateMilliseconds)) return;
		switch (Events.Phase)
		{
		case 0:
			Visit(Events.ChangedCoverage, [this](const FVoxelPublishGroupKey& Key)
			{
				if (Key.Representation == 4) { OnFineOwnershipChanged(Key.Coordinate); IndexCoverage(FVoxelViewKey{Key.Coordinate, 0}); MarkCoverageDirty(FineCoverageRect(Key.Coordinate)); }
				else if (Key.Representation == 1) { const FVoxelViewKey Spatial{Key.Coordinate, Key.Level}; IndexCoverage(Spatial); MarkCoverageDirty(VoxelProxyCoverageRect(Spatial)); }
				else if (Key.Representation == 2) { const FVoxelSurfaceTileKey Spatial{FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level}; IndexCoverage(Spatial); MarkCoverageDirty(SurfaceCoverageRect(Spatial)); }
				else if (Key.Representation == 3) { const FVoxelMacroTileKey Spatial{FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level}; IndexCoverage(Spatial); MarkCoverageDirty(MacroCoverageRect(Spatial)); }
			});
			break;
		case 1:
			Visit(Events.Retirements, [this](const FVoxelPublishGroupKey& Key)
			{
				const double Now = FPlatformTime::Seconds();
				QueueRetirement(Key);
				if (Key.Representation == 0) RetireFinePageLayoutState({Key.Coordinate, Key.Level});
				else if (Key.Representation == 4) { OnFineOwnershipChanged(Key.Coordinate); FineLastWanted.Add(Key.Coordinate, Now); IndexCoverage(FVoxelViewKey{Key.Coordinate, 0}); MarkCoverageDirty(FineCoverageRect(Key.Coordinate)); }
				else if (Key.Representation == 1) { const FVoxelViewKey Spatial{Key.Coordinate, Key.Level}; VoxelProxyLastWanted.Add(Spatial, Now); IndexCoverage(Spatial); MarkCoverageDirty(VoxelProxyCoverageRect(Spatial)); }
				else if (Key.Representation == 2) { const FVoxelSurfaceTileKey Spatial{FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level}; SurfaceLastWanted.Add(Spatial, Now); IndexCoverage(Spatial); MarkCoverageDirty(SurfaceCoverageRect(Spatial)); }
				else if (Key.Representation == 3) { const FVoxelMacroTileKey Spatial{FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level}; MacroLastWanted.Add(Spatial, Now); IndexCoverage(Spatial); MarkCoverageDirty(MacroCoverageRect(Spatial)); }
			});
			break;
		case 2:
			Visit(Events.LayoutChanges, [this](const FVoxelFinePageKey& Key) { InvalidateFinePageLayout(Key); });
			break;
		case 3:
			Visit(Events.RetiredShards, [this](const FVoxelFineShardKey& Key) { RetireFineShard(Key); });
			break;
		case 4:
			Visit(Events.ChangedShardOwners, [this](const FVoxelFineShardKey& Key)
			{
				OnOwnershipChanged(Key);
			});
			break;
		case 5:
			Visit(Events.Snapshot->PublicationInputs->Geometry, [this](const FVoxelTerrainGeometry& Geometry)
			{
				if (!Module.GetRuntime()->GetChangeHierarchy().RegisterGeometry(Geometry)) return;
				FIntVector Min, Max;
				if (Geometry.Key.TryBounds(Geometry.WorldMinZ, Geometry.WorldMaxZ, Min, Max)) MarkCoverageDirty({FIntPoint(Min.X, Min.Y), FIntPoint(Max.X, Max.Y)});
				bHeightfieldTopologyDirty |= Geometry.Key.IsColumn();
			});
			break;
		default:
			CancelStaleViewTasks();
			ReleaseInterestWorkspace(CommittedWorkspaceEvents);
			TerrainFrameBudget.Finish(EVoxelPublishPhase::Plan, Start, FPlatformTime::Seconds());
			return;
		}
		TerrainFrameBudget.Finish(EVoxelPublishPhase::Plan, Start, FPlatformTime::Seconds());
	}
}
