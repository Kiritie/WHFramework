#include "Voxel/Rendering/VoxelViewManager.h"

#include "Misc/ScopeExit.h"
#include "HAL/IConsoleManager.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Rendering/VoxelMeshClipper.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelTerrainRenderer.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "VoxelSeamPolicy.h"

void FVoxelViewManager::RestorePublicationGroups()
{
	for (const auto& Pair : ActiveCoveragePublishGroups)
	{
		const FGuid* Existing = OpenPublishGroups.Find(Pair.Key);
		check(!Existing || *Existing == Pair.Value);
		OpenPublishGroups.Add(Pair.Key, Pair.Value);
	}
	ActiveCoveragePublishGroups.Reset();
}

FVoxelGameThreadContinuation FVoxelViewManager::PublishTerrainLayoutSteps(FPublicationLayout InLayout)
{
	const double ResolveStart = FPlatformTime::Seconds();
	const auto ContentSnapshot = Module.GetRuntime()->GetChangeHierarchy().CaptureVersionSnapshot();
	const auto PresentedFine = CapturePresentedSnapshot();
	FPublicationSources Sources;
	Sources.bScoped = !GetInputRoot().bInitialLayout && !bActiveCoverageFull;
	auto& CoverageSnapshot = InLayout.CoverageSnapshot;
	auto& TargetTerrainNodes = InLayout.TargetTerrainNodes;
	auto& PreviousTerrainNodes = InLayout.PreviousTerrainNodes;
	auto& LocalFineKeys = InLayout.LocalFineKeys;
	auto& LocalProxyKeys = InLayout.LocalProxyKeys;
	auto& LocalSurfaceKeys = InLayout.LocalSurfaceKeys;
	auto& LocalMacroKeys = InLayout.LocalMacroKeys;
	const bool bInitialLayout = GetInputRoot().bInitialLayout;
	const auto IsFinalVolumeReady = [this](const FVoxelViewKey& Key)
	{
		if (!VoxelSeamPolicy::IsEnabled()) return true;
		const uint64* Desired = DesiredVolumeSignatures.Find(Key);
		const auto Context = DesiredVolumeContexts.FindRef(Key);
		return Desired && Context && AreBoundaryInputsCurrent(*Context) &&
			(VolumeTransitionSignatures.FindRef(Key) == *Desired || PreparedVolumeSignatures.FindRef(Key) == *Desired);
	};
	auto& LocalHeightfieldOwners = Sources.HeightfieldOwners;
	for (const auto& Key : LocalSurfaceKeys)
		LocalHeightfieldOwners.Add({EVoxelHeightfieldRepresentation::Surface, Key.Coordinate, Key.Level});
	for (const auto& Key : LocalMacroKeys)
		LocalHeightfieldOwners.Add({EVoxelHeightfieldRepresentation::Macro, Key.Coordinate, Key.Level});
	TMap<FVoxelViewKey, uint64> VolumeToCommit;
#if !UE_BUILD_SHIPPING
	TArray<FVoxelViewKey, TInlineAllocator<2>> PointHandoffKeys;
	TSet<FVoxelViewKey> PointReadyTargets;
	if (FPlatformTime::Seconds() >= NextPointHandoffLog)
	{
		for (const auto& Pair : GetViewInterest().Sources)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const auto& Source = Pair.Value.Source;
			if (!Source.bLocalView || !Source.Has(EVoxelStreamingCapability::FineVisual) || Source.Has(EVoxelStreamingCapability::LocalRefinement))
				continue;
			const auto AddPoint = [&](const FIntVector& Cell)
			{
				PointHandoffKeys.AddUnique(
					{FIntVector(VoxelGeneration::FloorDivide(Cell.X, 16), VoxelGeneration::FloorDivide(Cell.Y, 16), VoxelGeneration::FloorDivide(Cell.Z, 16)),
						0});
			};
			AddPoint(Source.Center);
			FVoxelNaturalColumnEntryPtr Column;
			if (Module.GetGenerationCache() && Module.GetGenerationCache()->FindNaturalColumn(FIntPoint(Source.Center.X, Source.Center.Y), Column))
				AddPoint(FIntVector(Source.Center.X, Source.Center.Y, Column->Column.SurfaceZ));
			break;
		}
		for (const auto& Key : PointHandoffKeys)
			if (TargetTerrainNodes.Contains(Key))
				PointReadyTargets.Add(Key);
		NextPointHandoffLog = FPlatformTime::Seconds() + 5.0;
	}
#endif
	const double PlanMilliseconds = (ResolveStart - InLayout.PlanningStartedSeconds) * 1000.0;
	auto& FineBoxes = Sources.FineBoxes;
	auto& CommonHeightfieldBoxes = Sources.HeightfieldBoxes;
	auto& CommonTransitionBoxes = Sources.TransitionBoxes;
	auto Box = [](const FVoxelCoverageBox& Bounds)
	{
		return FBox(FVector(Bounds.Min), FVector(Bounds.Max));
	};
	auto Column = [](const FVoxelCoverageRect& Bounds)
	{
		return FBox(FVector(Bounds.Min.X, Bounds.Min.Y, -1.e12), FVector(Bounds.Max.X, Bounds.Max.Y, 1.e12));
	};
	for (const auto& Rect : ActiveDirtyCoverageRects)
		Sources.ScopeBoxes.Add(Column(Rect));
	const double HeightfieldPlanStarted = FPlatformTime::Seconds();
	if (bHeightfieldTopologyDirty)
	{
		bHeightfieldTopologyDirty = false;
		co_await RebuildHeightfieldTransitionSteps(CoverageSnapshot);
		if (bCoverageRetryRequested)
		{
			bHeightfieldTopologyDirty = true;
			co_return;
		}
		++HeightfieldTopologyRebuildCount;
	}

	const double HeightfieldPlanMilliseconds = (FPlatformTime::Seconds() - HeightfieldPlanStarted) * 1000.0;
	auto& HeightfieldToCommit = Sources.HeightfieldToCommit;
	for (const auto& LocalOwner : LocalHeightfieldOwners)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto* Prepared = PreparedTransitionSignatures.Find(LocalOwner);
		if (!Prepared)
			continue;
		const TPair<FVoxelHeightfieldNodeKey, uint64> Pair(LocalOwner, *Prepared);
		const uint64* Desired = DesiredTransitionSignatures.Find(Pair.Key);
		const auto Mesh = PreparedTransitionMeshes.FindRef(Pair.Key);
		const FVoxelCoverageRect Rect = Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
											? SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level})
											: MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level});
		if (!Desired || *Desired != Pair.Value || !Mesh || PendingHeightfieldPlanOwners.Contains(Pair.Key) || VolumeHandoffAffectsHeightfield(Pair.Key))
			continue;
		const int32 TileSide = Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
								   ? VoxelViewLod::SurfaceSide(Module.GetViewSettings().Lod, Pair.Key.Level)
								   : VoxelViewLod::MacroSide(Module.GetViewSettings().Lod, Pair.Key.Level);
		FVoxelViewSourceId SourceId = HeightfieldTransitionSources.FindRef(Pair.Key);
		const FVoxelPublishGroupKey GroupKey{static_cast<uint8>(Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 4 : 5),
			Pair.Key.Coordinate, Pair.Key.Level};
		if (bInitialLayout && !ActiveCoveragePublishGroups.Contains(GroupKey) && Publisher->IsGroupBusy(GroupKey)) continue;
		const FGuid Group = BeginPublishGroup(GroupKey);
		if (HasRenderableMesh(*Mesh) || SourceId)
		{
			if (!Publisher->StageImmutable(Group, SourceId,
					FVector(Pair.Key.Coordinate.X * TileSide, Pair.Key.Coordinate.Y * TileSide, 0) * Module.BlockSize(), Module.BlockSize(), Mesh,
					Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 2 : 3, true))
			{
				MarkPublishGroupCoverageDirty(FVoxelPublishGroupKey{
					static_cast<uint8>(Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 4 : 5), Pair.Key.Coordinate, Pair.Key.Level});
				continue;
			}
			HeightfieldTransitionSources.Add(Pair.Key, SourceId);
			co_await UpdatePublicationCoverageSteps(Sources, Group, SourceId, {});
		}
		HeightfieldToCommit.Add(Pair.Key, Pair.Value);
	}
	auto& BlockedHeightfieldNodes = Sources.BlockedHeightfieldOwners;
	for (const auto& Owner : LocalHeightfieldOwners)
		if (PendingHeightfieldPlanOwners.Contains(Owner))
			BlockedHeightfieldNodes.Add(Owner);
	for (const auto& LocalOwner : LocalHeightfieldOwners)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const TPair<FVoxelHeightfieldNodeKey, int32> Pair(LocalOwner, 0);
		const FVoxelCoverageRect Rect = Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
											? SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level})
											: MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level});
		if (VolumeHandoffAffectsHeightfield(Pair.Key))
			BlockedHeightfieldNodes.Add(Pair.Key);
	}
	for (const auto& LocalOwner : LocalHeightfieldOwners)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto* Edges = DesiredTransitionEdges.Find(LocalOwner);
		if (!Edges)
			continue;
		const TPair<FVoxelHeightfieldNodeKey, const TArray<FVoxelHeightfieldTransitionEdge>&> Pair(LocalOwner, *Edges);
		const uint64* Desired = DesiredTransitionSignatures.Find(Pair.Key);
		const uint64* Staged = HeightfieldToCommit.Find(Pair.Key);
		const bool bReady = IsHeightfieldTransitionPresented(Pair.Key) || (Desired && Staged && *Staged == *Desired);
		if (bReady)
			continue;
		BlockedHeightfieldNodes.Add(Pair.Key);
		for (const FVoxelHeightfieldTransitionEdge& Edge : Pair.Value)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (!Edge.bPending)
				BlockedHeightfieldNodes.Add(Edge.Neighbor);
		}
	}
	if (bActiveCoverageFull)
		PendingHeightfieldHandoffRects.Reset();
	else
		PendingHeightfieldHandoffRects.RemoveAll(
			[this](const FVoxelCoverageRect& Rect)
			{
				return CoverageAffects(Rect);
			});
	for (const FVoxelHeightfieldNodeKey& Key : BlockedHeightfieldNodes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Key.Representation == EVoxelHeightfieldRepresentation::Surface)
		{
			const FVoxelSurfaceTileKey Tile{Key.Coordinate, Key.Level};
			if (GetInputRoot().SurfaceWanted.Contains(Tile))
				PendingHeightfieldHandoffRects.Add(SurfaceCoverageRect(Tile));
		}
		else
		{
			const FVoxelMacroTileKey Tile{Key.Coordinate, Key.Level};
			if (GetInputRoot().MacroWanted.Contains(Tile))
				PendingHeightfieldHandoffRects.Add(MacroCoverageRect(Tile));
		}
	}
	for (const auto& LocalOwner : LocalProxyKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto* Prepared = PreparedVolumeSignatures.Find(LocalOwner);
		if (!Prepared)
			continue;
		const TPair<FVoxelViewKey, uint64> Pair(LocalOwner, *Prepared);
		const uint64* Desired = DesiredVolumeSignatures.Find(Pair.Key);
		const auto Mesh = PreparedVolumeMeshes.FindRef(Pair.Key);
		if (!Desired || *Desired != Pair.Value || !Mesh || !TargetTerrainNodes.Contains(Pair.Key) ||
			(bInitialLayout ? !IsFinalVolumeReady(Pair.Key) :
				VoxelCoverage::IntersectsAny3D(VoxelProxyCoverageBox(Pair.Key), PendingVolumeHandoffBoxes)))
			continue;
		// 空接缝结果也必须提交签名，释放准备结果并停止重复覆盖计算。
		const FVoxelPublishGroupKey GroupKey{1, Pair.Key.Coordinate, Pair.Key.Level};
		if (bInitialLayout && !ActiveCoveragePublishGroups.Contains(GroupKey) && Publisher->IsGroupBusy(GroupKey)) continue;
		const FGuid Group = BeginPublishGroup(GroupKey);
		const int32 TerrainStage = VolumeTransitionWorkClass(Pair.Key) <= EVoxelWorkClass::Interactive ? 0 : 1;
		Publisher->SetTerrainStage(Group, TerrainStage);
		FVoxelViewSourceId SourceId = VoxelProxySources.FindRef(Pair.Key);
		if (HasRenderableMesh(*Mesh) || SourceId)
		{
			if (!Publisher->StageImmutable(Group, SourceId, FVector(Pair.Key.GetBounds().Min) * Module.BlockSize(),
					Module.BlockSize() * VoxelProxyData.FindChecked(Pair.Key)->Geometry.SampleStepCells, Mesh, TerrainStage))
			{
				PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Pair.Key));
				MarkCoverageDirty(VoxelProxyCoverageRect(Pair.Key));
				continue;
			}
			VoxelProxySources.Add(Pair.Key, SourceId);
		}
		if (const auto BoundaryMesh = PreparedVolumeBoundaryMeshes.FindRef(Pair.Key))
		{
			FVoxelViewSourceId BoundarySourceId = VolumeBoundarySources.FindRef(Pair.Key);
			if (HasRenderableMesh(*BoundaryMesh) || BoundarySourceId)
			{
				if (!Publisher->StageImmutable(Group, BoundarySourceId, FVector(Pair.Key.GetBounds().Min) * Module.BlockSize(),
						Module.BlockSize() * VoxelProxyData.FindChecked(Pair.Key)->Geometry.SampleStepCells, BoundaryMesh, TerrainStage, true))
					continue;
				VolumeBoundarySources.Add(Pair.Key, BoundarySourceId);
				co_await UpdatePublicationCoverageSteps(Sources, Group, BoundarySourceId, {});
				Publisher->SetHidden(Group, BoundarySourceId, false);
			}
		}
		VolumeToCommit.Add(Pair.Key, Pair.Value);
	}
	for (auto It = TargetTerrainNodes.CreateIterator(); It; ++It)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const FVoxelCoverageBox Bounds = It->Level == 0 ? FineCoverageBox(It->Coordinate) : VoxelProxyCoverageBox(*It);
		if (bInitialLayout ? (It->Level > 0 && !IsFinalVolumeReady(*It)) :
			VoxelCoverage::IntersectsAny3D(Bounds, PendingVolumeHandoffBoxes))
			It.RemoveCurrent();
	}
	for (const auto& Key : PreviousTerrainNodes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const FVoxelCoverageBox Bounds = Key.Level == 0 ? FineCoverageBox(Key.Coordinate) : VoxelProxyCoverageBox(Key);
		if (bInitialLayout ? (Key.Level > 0 && !IsFinalVolumeReady(Key)) :
			VoxelCoverage::IntersectsAny3D(Bounds, PendingVolumeHandoffBoxes))
			TargetTerrainNodes.Add(Key);
	}
#if !UE_BUILD_SHIPPING
	for (const auto& Key : PointHandoffKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		UE_LOG(LogTemp, Display,
			TEXT(
				"Voxel point handoff: section=%s ready=%d desired=%d readyTarget=%d volumeBlocked=%d target=%d owned=%d presented=%d"),
			*Key.Coordinate.ToString(), FineReady.Contains(Key.Coordinate), GetInputRoot().FineWanted.Contains(Key.Coordinate), PointReadyTargets.Contains(Key),
			VoxelCoverage::IntersectsAny3D(FineCoverageBox(Key.Coordinate), PendingVolumeHandoffBoxes), TargetTerrainNodes.Contains(Key),
			VisibleTerrainNodes.Contains(Key), IsFineCoverageReady(Key.Coordinate, PresentedFine.Get()));
	}
#endif
	auto& RecheckCoverageRects = Sources.RecheckCoverageRects;
	for (const auto& Pair : VolumeToCommit)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		RecheckCoverageRects.Add(VoxelProxyCoverageRect(Pair.Key));
	}
	for (const auto& Pair : HeightfieldToCommit)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		RecheckCoverageRects.Add(Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
									 ? SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level})
									 : MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level}));
	}
	const double NormalizeStarted = FPlatformTime::Seconds();
	if (!bInitialLayout)
	{
		const auto DynamicFine = MakeShared<TArray<FBox>, ESPMode::ThreadSafe>();
		for (const FIntVector& Key : LocalFineKeys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (GetInputRoot().FineWanted.Contains(Key) && IsFineCoverageReady(Key, PresentedFine.Get()))
				DynamicFine->Add(Box(FineCoverageBox(Key)));
		}
		co_await VoxelMeshClipper::NormalizeBoxesSteps(*DynamicFine);
		FineBoxes = CommonHeightfieldBoxes = DynamicFine;
		const auto DynamicTransition = MakeShared<TArray<FBox>, ESPMode::ThreadSafe>();
		for (FBox Bounds : *DynamicFine)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			Bounds.Min.Z = -1.e12;
			Bounds.Max.Z = 1.e12;
			DynamicTransition->Add(Bounds);
		}
		co_await VoxelMeshClipper::NormalizeBoxesSteps(*DynamicTransition);
		CommonTransitionBoxes = DynamicTransition;
	}
	const double NormalizeMilliseconds = (FPlatformTime::Seconds() - NormalizeStarted) * 1000.0;
	const double BoxesMilliseconds = (FPlatformTime::Seconds() - ResolveStart) * 1000.0;
	if (bInitialLayout)
		co_await PublishInitialSourcesSteps(InLayout, Sources);
	else
		co_await PublishDynamicSourcesSteps(InLayout, Sources);
	const double SourcesMilliseconds = (FPlatformTime::Seconds() - ResolveStart) * 1000.0 - BoxesMilliseconds;
	TMap<FVoxelPublishGroupKey, TArray<FVoxelViewKey>> PreviousNodesByGroup;
	TMap<FVoxelPublishGroupKey, TArray<FVoxelViewKey>> TargetNodesByGroup;
	const auto TerrainGroupKey = [](const FVoxelViewKey& Key)
	{
		return FVoxelPublishGroupKey{1, Key.Coordinate, Key.Level};
	};
	for (const FVoxelViewKey& Key : PreviousTerrainNodes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Key.Level == 0 || !CoverageAffects(TerrainCoverageRect(Key)))
			continue;
		if (!TargetTerrainNodes.Contains(Key))
			BeginPublishGroup(TerrainGroupKey(Key));
		if (ActiveCoveragePublishGroups.Contains(TerrainGroupKey(Key)))
			PreviousNodesByGroup.FindOrAdd(TerrainGroupKey(Key)).Add(Key);
	}
	for (const FVoxelViewKey& Key : TargetTerrainNodes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Key.Level == 0 || !CoverageAffects(TerrainCoverageRect(Key)))
			continue;
		if (!VisibleTerrainNodes.Contains(Key) && (!bInitialLayout ||
			ActiveCoveragePublishGroups.Contains(TerrainGroupKey(Key)) || !Publisher->IsGroupBusy(TerrainGroupKey(Key))))
			BeginPublishGroup(TerrainGroupKey(Key));
		if (ActiveCoveragePublishGroups.Contains(TerrainGroupKey(Key)))
			TargetNodesByGroup.FindOrAdd(TerrainGroupKey(Key)).Add(Key);
	}
	TMap<FVoxelPublishGroupKey, TArray<FVoxelViewKey>> VolumeByGroup;
	for (const auto& Pair : VolumeToCommit)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		VolumeByGroup.FindOrAdd(TerrainGroupKey(Pair.Key)).Add(Pair.Key);
	}
	TMap<FVoxelPublishGroupKey, TArray<FVoxelHeightfieldNodeKey>> HeightfieldByGroup;
	for (const auto& Pair : HeightfieldToCommit)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const FVoxelHeightfieldNodeKey& Key = Pair.Key;
		const uint8 Representation = Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 4 : 5;
		HeightfieldByGroup.FindOrAdd({Representation, Key.Coordinate, Key.Level}).Add(Key);
	}
	TSet<FVoxelPublishGroupKey> OwnershipChanges;
	for (const auto& Pair : ActiveCoveragePublishGroups)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto* Previous = PreviousNodesByGroup.Find(Pair.Key);
		const auto* Target = TargetNodesByGroup.Find(Pair.Key);
		bool bChanged = (Previous ? Previous->Num() : 0) != (Target ? Target->Num() : 0);
		if (!bChanged && Previous && Target)
			for (const auto& Node : *Previous)
				if (!Target->Contains(Node))
				{
					bChanged = true;
					break;
				}
		if (bChanged || VolumeByGroup.Contains(Pair.Key) || HeightfieldByGroup.Contains(Pair.Key))
			OwnershipChanges.Add(Pair.Key);
	}
	TFunction<void()> OnPublishCommitted = [this, Incarnation = TaskOwnerIncarnation, LocalOwners = MoveTemp(LocalHeightfieldOwners), Recheck = MoveTemp(RecheckCoverageRects)]() mutable
	{
		if (Incarnation != TaskOwnerIncarnation) return;
		DeferredCoverageCallbacks.Enqueue({{}, RetirePublishedHeightfieldSteps(Incarnation, MoveTemp(LocalOwners), MoveTemp(Recheck))});
	};
	TMap<FVoxelTerrainSpatialKey, FVoxelTerrainGeometryMetrics> GeneratedForCommit;
	for (const auto& Group : ActiveCoveragePublishGroups)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Group.Key.Representation == 0 || Group.Key.Representation > 3)
			continue;
		const auto Representation = static_cast<EVoxelTerrainRepresentation>(Group.Key.Representation + 1);
		const FVoxelTerrainSpatialKey Key{Representation, Group.Key.Coordinate, Group.Key.Level};
		if (const auto* Metrics = GeometryMetrics.Find(Key))
			GeneratedForCommit.Add(Key, *Metrics);
	}
	TFunction<void(TConstArrayView<FVoxelPublishGroupKey>)> OnGroupCommitted =
		[this, Incarnation = TaskOwnerIncarnation, Generated = MoveTemp(GeneratedForCommit), Target = MoveTemp(TargetNodesByGroup),
			Volume = MoveTemp(VolumeByGroup), Heightfield = MoveTemp(HeightfieldByGroup),
			VolumeSignatures = MoveTemp(VolumeToCommit), HeightfieldSignatures = MoveTemp(HeightfieldToCommit)](TConstArrayView<FVoxelPublishGroupKey> Keys)
	{
		if (Incarnation != TaskOwnerIncarnation) return;
		bTerrainBuildStageDirty = true;
		for (const FVoxelPublishGroupKey& Key : Keys)
		{
			const auto* Desired = Target.Find(Key);
			if (Key.Representation == 1 && CommitTerrainOwnership(Key, Desired ? MakeArrayView(*Desired) : TConstArrayView<FVoxelViewKey>()))
			{
				MarkCoverageDirty(VoxelProxyCoverageRect({Key.Coordinate, Key.Level}));
				bHeightfieldTopologyDirty |= !GetInputRoot().bInitialLayout;
			}
			if (Key.Representation >= 1 && Key.Representation <= 3)
			{
				const FVoxelTerrainSpatialKey Spatial{static_cast<EVoxelTerrainRepresentation>(Key.Representation + 1), Key.Coordinate, Key.Level};
				FVoxelViewSourceId SourceId = Key.Representation == 1 ? VoxelProxySources.FindRef({Key.Coordinate, Key.Level})
								: Key.Representation == 2 ? SurfaceSources.FindRef({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level})
														  : MacroSources.FindRef({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
				if (const auto* Metrics = Generated.Find(Spatial))
				{
					auto Presented = *Metrics;
					Presented.bPresented = Publisher->GetPresentedSourceMeshCost(SourceId, Presented.Vertices, Presented.Triangles, Presented.MeshBytes);
					if (Presented.bPresented)
						PresentedGeometryMetrics.Add(Spatial, Presented);
					else
						PresentedGeometryMetrics.Remove(Spatial);
				}
				else if (!SourceId || Publisher->IsHidden(SourceId))
					PresentedGeometryMetrics.Remove(Spatial);
			}
			if (const TArray<FVoxelViewKey>* Nodes = Volume.Find(Key))
			{
				for (const FVoxelViewKey& Node : *Nodes)
				{
					if (const uint64* Signature = VolumeSignatures.Find(Node))
					{
						VolumeTransitionSignatures.Add(Node, *Signature);
						const uint64* Prepared = PreparedVolumeSignatures.Find(Node);
						if (Prepared && *Prepared == *Signature)
						{
							PreparedVolumeSignatures.Remove(Node);
							PreparedVolumeMeshes.Remove(Node);
							PreparedVolumeBoundaryMeshes.Remove(Node);
						}
						MarkCoverageDirty(VoxelProxyCoverageRect(Node));
					}
				}
			}
			if (const TArray<FVoxelHeightfieldNodeKey>* Nodes = Heightfield.Find(Key))
			{
				for (const FVoxelHeightfieldNodeKey& Node : *Nodes)
				{
					if (const uint64* Signature = HeightfieldSignatures.Find(Node))
					{
						const uint64* DesiredSignature = DesiredTransitionSignatures.Find(Node);
						const FVoxelViewSourceId SourceId = HeightfieldTransitionSources.FindRef(Node);
						if (!DesiredSignature || *DesiredSignature != *Signature ||
							(SourceId && (!Publisher->IsCommitted(SourceId) || Publisher->IsHidden(SourceId))))
							continue;
						HeightfieldTransitionSignatures.Add(Node, *Signature);
						const uint64* Prepared = PreparedTransitionSignatures.Find(Node);
						if (Prepared && *Prepared == *Signature)
						{
							PreparedTransitionSignatures.Remove(Node);
							PreparedTransitionMeshes.Remove(Node);
						}
						MarkCoverageDirty(Node.Representation == EVoxelHeightfieldRepresentation::Surface ? SurfaceCoverageRect({Node.Coordinate, Node.Level})
																										  : MacroCoverageRect({Node.Coordinate, Node.Level}));
					}
				}
			}
		}
	};
	for (auto It = ActiveCoveragePublishGroups.CreateIterator(); It; ++It)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		Publisher->SetOwnershipChanged(It.Value(), OwnershipChanges.Contains(It.Key()), bInitialLayout);
		if (Publisher->DiscardUnchangedGroup(It.Value()))
			It.RemoveCurrent();
	}
	if (ActiveCoveragePublishGroups.IsEmpty())
	{
		OnPublishCommitted();
	}
	else
	{
		TArray<FGuid> Handles;
		Handles.Reserve(ActiveCoveragePublishGroups.Num());
		for (const auto& Pair : ActiveCoveragePublishGroups) Handles.Add(Pair.Value);
		Publisher->SetCommitCallbacks(MoveTemp(OnPublishCommitted), MoveTemp(OnGroupCommitted), Handles);
		const FVoxelCoverageIndex FrozenCoverage = bInitialLayout ? FVoxelCoverageIndex{} : CoverageIndex;
		const TWeakPtr<FInterestInputRoot, ESPMode::ThreadSafe> InitialPartition = bInitialLayout ?
			(bCoverageReadScope && CoverageInputRoot ? CoverageInputRoot : AppliedInputRoot) : nullptr;
		for (const auto& Pair : ActiveCoveragePublishGroups)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const auto Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Pair.Key, Module.BlockSize(), Module.GetViewSettings().Lod);
			FVoxelGenerationBounds GuardBounds;
			if (Bounds.IsValid)
			{
				const bool bVolume = Pair.Key.Representation <= 1;
				const int32 MinZ = bVolume ? FMath::FloorToInt(Bounds.Min.Z / Module.BlockSize()) - 1 : MIN_int32 + 1;
				const int32 MaxZ = bVolume ? FMath::CeilToInt(Bounds.Max.Z / Module.BlockSize()) + 1 : MAX_int32;
				GuardBounds = {
					FIntVector(FMath::FloorToInt(Bounds.Min.X / Module.BlockSize()) - 1, FMath::FloorToInt(Bounds.Min.Y / Module.BlockSize()) - 1, MinZ),
					FIntVector(FMath::CeilToInt(Bounds.Max.X / Module.BlockSize()) + 1, FMath::CeilToInt(Bounds.Max.Y / Module.BlockSize()) + 1, MaxZ)};
			}
			const uint64 Epoch = WorldEpoch;
			const FVoxelGenerationBounds DependencyBounds = Bounds.IsValid ? FVoxelGenerationBounds{
				FIntVector(FMath::FloorToInt(Bounds.Min.X / Module.BlockSize()), FMath::FloorToInt(Bounds.Min.Y / Module.BlockSize()),
					Pair.Key.Representation <= 1 ? FMath::FloorToInt(Bounds.Min.Z / Module.BlockSize()) : Module.GetManifest().Settings.MinZ),
				FIntVector(FMath::CeilToInt(Bounds.Max.X / Module.BlockSize()), FMath::CeilToInt(Bounds.Max.Y / Module.BlockSize()),
					Pair.Key.Representation <= 1 ? FMath::CeilToInt(Bounds.Max.Z / Module.BlockSize()) : Module.GetManifest().Settings.MaxZ)} : FVoxelGenerationBounds{};
			Publisher->SetValidation(Pair.Value,
				[this, Epoch, ContentSnapshot, DependencyBounds, GuardBounds, FrozenCoverage, bInitialLayout, InitialPartition]
				{
					const bool bPartitionCurrent = bInitialLayout ? InitialPartition.IsValid() && AppliedInputRoot == InitialPartition.Pin() :
						!GuardBounds.IsValid() || CoverageIndex.IsRangeCurrent(FrozenCoverage, GuardBounds.Min, GuardBounds.Max);
					return Epoch == WorldEpoch && bPartitionCurrent && (!DependencyBounds.IsValid() ||
						Module.GetRuntime()->GetChangeHierarchy().IsVersionSnapshotCurrent(ContentSnapshot, DependencyBounds));
				});
			if (!Publisher->CommitGroup(Pair.Value))
				MarkPublishGroupCoverageDirty(Pair.Key);
		}
		ActiveCoveragePublishGroups.Reset();
	}
	const double Now = FPlatformTime::Seconds();
	if (IConsoleManager::Get().FindConsoleVariable(TEXT("wh.Voxel.DebugRepresentation"))->GetInt() > 0 && Now >= NextTimingLog)
	{
		UE_LOG(LogTemp, Display, TEXT("Voxel layout phase wall ms: plan=%.3f masksAndTopology=%.3f sources=%.3f commit=%.3f"), PlanMilliseconds, BoxesMilliseconds,
			SourcesMilliseconds, (Now - ResolveStart) * 1000.0 - BoxesMilliseconds - SourcesMilliseconds);
		// boxes 总阶段还包含高度场拓扑及交接；单独计量，避免误判为排序耗时。
		UE_LOG(LogTemp, Display, TEXT("Voxel layout masks elapsed ms: heightfieldPlan=%.3f masks=%.3f fine=%d heightfield=%d transition=%d initialLayout=%d fullMaskInputs=%d"),
			HeightfieldPlanMilliseconds, NormalizeMilliseconds,
			FineBoxes ? FineBoxes->Num() : 0, CommonHeightfieldBoxes ? CommonHeightfieldBoxes->Num() : 0,
			CommonTransitionBoxes ? CommonTransitionBoxes->Num() : 0,
			bInitialLayout, bActiveCoverageFull || bInitialLayout);
		NextTimingLog = Now + 5.0;
	}
}

FVoxelGameThreadContinuation FVoxelViewManager::PublishDynamicSourcesSteps(FPublicationLayout& InLayout, FPublicationSources& InSources)
{
	check(!GetInputRoot().bInitialLayout);
	auto& TargetTerrainNodes = InLayout.TargetTerrainNodes;
	auto& TerrainSourceCandidates = InLayout.TerrainSourceCandidates;
	auto& LocalSurfaceKeys = InLayout.LocalSurfaceKeys;
	auto& LocalMacroKeys = InLayout.LocalMacroKeys;
	auto& LocalHeightfieldOwners = InSources.HeightfieldOwners;
	auto& BlockedHeightfieldNodes = InSources.BlockedHeightfieldOwners;
	auto& HeightfieldToCommit = InSources.HeightfieldToCommit;
	auto& RecheckCoverageRects = InSources.RecheckCoverageRects;
	const auto FineBoxes = InSources.FineBoxes;
	const auto CommonHeightfieldBoxes = InSources.HeightfieldBoxes;
	const auto CommonTransitionBoxes = InSources.TransitionBoxes;
	const auto& CoverageSnapshot = InLayout.CoverageSnapshot;
	const auto Box = [](const FVoxelCoverageBox& Bounds) { return FBox(FVector(Bounds.Min), FVector(Bounds.Max)); };
	const auto Column = [](const FVoxelCoverageRect& Bounds) { return FBox(FVector(Bounds.Min.X, Bounds.Min.Y, -1.e12), FVector(Bounds.Max.X, Bounds.Max.Y, 1.e12)); };
	const auto HandoffPending = [this](const FVoxelCoverageRect& Rect) { return VoxelCoverage::IntersectsAny2D(Rect, PendingHeightfieldHandoffRects); };
	TArray<FVoxelCoverageRect> PresentedHeightfieldRects;
	for (const auto& Key : LocalSurfaceKeys)
		if (FVoxelViewSourceId SourceId = SurfaceSources.FindRef(Key); SourceId && !Publisher->IsHidden(SourceId) && Publisher->IsPresented(SourceId))
			PresentedHeightfieldRects.Add(SurfaceCoverageRect(Key));
	for (const auto& Key : LocalMacroKeys)
		if (FVoxelViewSourceId SourceId = MacroSources.FindRef(Key); SourceId && !Publisher->IsHidden(SourceId) && Publisher->IsPresented(SourceId))
			PresentedHeightfieldRects.Add(MacroCoverageRect(Key));

	TerrainCandidateCount = TerrainSourceCandidates.Num();
	const auto& ProxyExclusionKeys = TargetTerrainNodes;
	const auto& SurfaceExclusionKeys = LocalSurfaceKeys;
	const auto& MacroExclusionKeys = LocalMacroKeys;
	for (const FVoxelViewKey& Key : TerrainSourceCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Key.Level == 0)
			continue;
		FVoxelViewSourceId SourceId = VoxelProxySources.FindRef(Key);
		if (!SourceId || !CoverageAffects(VoxelProxyCoverageRect(Key)))
			continue;
		const bool bVisible = TargetTerrainNodes.Contains(Key);
		const FVoxelPublishGroupKey GroupKey{1, Key.Coordinate, Key.Level};
		if (!bVisible && Publisher->IsHidden(SourceId) && Publisher->IsCommitted(SourceId) && !Publisher->IsGroupBusy(GroupKey))
			continue;
		const FGuid Group = BeginPublishGroup(GroupKey);
		if (VoxelCoverage::IntersectsAny3D(VoxelProxyCoverageBox(Key), PendingVolumeHandoffBoxes))
		{
			if (!Publisher->HasPresentation(SourceId))
				Publisher->SetHidden(Group, SourceId, true);
			continue;
		}
		if (bVisible)
		{
			TArray<FBox> Exclusions = *FineBoxes;
			for (const auto& Other : ProxyExclusionKeys)
				if (Other.Level > 0 && Other.Level < Key.Level)
					Exclusions.Add(Box(VoxelProxyCoverageBox(Other)));
			co_await UpdatePublicationCoverageSteps(InSources, Group, SourceId, MoveTemp(Exclusions));
		}
		Publisher->SetHidden(Group, SourceId, !bVisible);
		if (FVoxelViewSourceId Boundary = VolumeBoundarySources.FindRef(Key))
			Publisher->SetHidden(Group, Boundary, !bVisible);
	}
	for (const FVoxelSurfaceTileKey& LocalKey : LocalSurfaceKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const TPair<FVoxelSurfaceTileKey, FVoxelViewSourceId> Pair(LocalKey, SurfaceSources.FindRef(LocalKey));
		if (!Pair.Value)
			continue;
		if (!CoverageAffects(SurfaceCoverageRect(Pair.Key)))
			continue;
		const FGuid Group = BeginPublishGroup({2, Pair.Key.Coordinate, Pair.Key.Level});
		const bool bHasPreviousCoverage =
			Publisher->HasPresentation(Pair.Value) || VoxelCoverage::IntersectsAny2D(SurfaceCoverageRect(Pair.Key), PresentedHeightfieldRects);
		if (bHasPreviousCoverage && (BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Surface, Pair.Key.Coordinate, Pair.Key.Level}) ||
										(!GetInputRoot().SurfaceWanted.Contains(Pair.Key) && HandoffPending(SurfaceCoverageRect(Pair.Key)))))
		{
			if (!Publisher->HasPresentation(Pair.Value))
				Publisher->SetHidden(Group, Pair.Value, true);
			else
				co_await UpdatePublicationCoverageSteps(InSources, Group, Pair.Value, {}, CommonHeightfieldBoxes, true);
			if (FVoxelViewSourceId Water = WaterSources.FindRef(Pair.Key))
				co_await UpdatePublicationCoverageSteps(InSources, Group, Water, {}, CommonHeightfieldBoxes, true);
			continue;
		}
		const bool bWanted = GetInputRoot().SurfaceWanted.Contains(Pair.Key);
		bool bHidden = false;
		if (!bWanted)
			co_await VoxelCoverage::IsFullyCovered2DSteps(SurfaceCoverageRect(Pair.Key), CoverageSnapshot.Macro, bHidden);
		if (!bWanted && !bHidden)
			co_await VoxelCoverage::IsFullyCovered2DSteps(SurfaceCoverageRect(Pair.Key), CoverageSnapshot.ProxySurface, bHidden);
		const FVoxelCoverageRect SourceRect = SurfaceCoverageRect(Pair.Key);
		if (!CoverageAffects(SourceRect))
		{
			Publisher->SetHidden(Group, Pair.Value, bHidden);
			if (FVoxelViewSourceId Water = WaterSources.FindRef(Pair.Key))
				Publisher->SetHidden(Group, Water, bHidden);
			continue;
		}
		TArray<FBox> Exclusions;
		for (const FVoxelSurfaceTileKey& Key : SurfaceExclusionKeys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if ((SurfaceReady.Contains(Key) &&
				!BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Surface, Key.Coordinate, Key.Level})) &&
				GetInputRoot().SurfaceWanted.Contains(Key) && Key != Pair.Key &&
				(!bWanted || Key.Level < Pair.Key.Level))
				if (SourceRect.Intersects(SurfaceCoverageRect(Key)))
					Exclusions.Add(Column(SurfaceCoverageRect(Key)));
		}
		if (!bWanted)
		{
			for (const FVoxelMacroTileKey& Key : LocalMacroKeys)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				if (MacroReady.Contains(Key) && GetInputRoot().MacroWanted.Contains(Key) &&
					!BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Macro, Key.Coordinate, Key.Level}))
					if (SourceRect.Intersects(MacroCoverageRect(Key)))
						Exclusions.Add(Column(MacroCoverageRect(Key)));
			}
		}
		if (!bHidden)
		{
			if (!Publisher->IsCommitted(Pair.Value))
				RecheckCoverageRects.Add(SourceRect);
			co_await UpdatePublicationCoverageSteps(InSources, Group, Pair.Value, Exclusions, CommonHeightfieldBoxes);
			if (FVoxelViewSourceId Water = WaterSources.FindRef(Pair.Key))
				co_await UpdatePublicationCoverageSteps(InSources, Group, Water, MoveTemp(Exclusions), CommonHeightfieldBoxes);
		}
		Publisher->SetHidden(Group, Pair.Value, bHidden);
		if (FVoxelViewSourceId Water = WaterSources.FindRef(Pair.Key))
			Publisher->SetHidden(Group, Water, bHidden);
	}
	for (const FVoxelMacroTileKey& LocalKey : LocalMacroKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const TPair<FVoxelMacroTileKey, FVoxelViewSourceId> Pair(LocalKey, MacroSources.FindRef(LocalKey));
		if (!Pair.Value)
			continue;
		if (!CoverageAffects(MacroCoverageRect(Pair.Key)))
			continue;
		const FGuid Group = BeginPublishGroup({3, Pair.Key.Coordinate, Pair.Key.Level});
		const bool bHasPreviousCoverage =
			Publisher->HasPresentation(Pair.Value) || VoxelCoverage::IntersectsAny2D(MacroCoverageRect(Pair.Key), PresentedHeightfieldRects);
		if (bHasPreviousCoverage && (BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Macro, Pair.Key.Coordinate, Pair.Key.Level}) ||
										(!GetInputRoot().MacroWanted.Contains(Pair.Key) && HandoffPending(MacroCoverageRect(Pair.Key)))))
		{
			if (!Publisher->HasPresentation(Pair.Value))
				Publisher->SetHidden(Group, Pair.Value, true);
			else
				co_await UpdatePublicationCoverageSteps(InSources, Group, Pair.Value, {}, CommonHeightfieldBoxes, true);
			continue;
		}
		const bool bWanted = GetInputRoot().MacroWanted.Contains(Pair.Key);
		bool bHidden = false;
		if (!bWanted)
			co_await VoxelCoverage::IsFullyCovered2DSteps(MacroCoverageRect(Pair.Key), CoverageSnapshot.Surface, bHidden);
		const FVoxelCoverageRect SourceRect = MacroCoverageRect(Pair.Key);
		if (!CoverageAffects(SourceRect))
		{
			Publisher->SetHidden(Group, Pair.Value, bHidden);
			continue;
		}
		TArray<FBox> Exclusions;
		for (const FVoxelSurfaceTileKey& Key : SurfaceExclusionKeys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if ((SurfaceReady.Contains(Key) &&
				!BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Surface, Key.Coordinate, Key.Level})) &&
				GetInputRoot().SurfaceWanted.Contains(Key))
				if (SourceRect.Intersects(SurfaceCoverageRect(Key)))
					Exclusions.Add(Column(SurfaceCoverageRect(Key)));
		}
		for (const FVoxelMacroTileKey& Key : MacroExclusionKeys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if ((MacroReady.Contains(Key) &&
				!BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Macro, Key.Coordinate, Key.Level})) &&
				GetInputRoot().MacroWanted.Contains(Key) && Key != Pair.Key &&
				(!bWanted || Key.Level < Pair.Key.Level))
				if (SourceRect.Intersects(MacroCoverageRect(Key)))
					Exclusions.Add(Column(MacroCoverageRect(Key)));
		}
		if (!bHidden)
		{
			if (!Publisher->IsCommitted(Pair.Value))
				RecheckCoverageRects.Add(SourceRect);
			co_await UpdatePublicationCoverageSteps(InSources, Group, Pair.Value, MoveTemp(Exclusions), CommonHeightfieldBoxes);
		}
		Publisher->SetHidden(Group, Pair.Value, bHidden);
	}
	for (const auto& LocalOwner : LocalHeightfieldOwners)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const TPair<FVoxelHeightfieldNodeKey, FVoxelViewSourceId> Pair(LocalOwner, HeightfieldTransitionSources.FindRef(LocalOwner));
		if (!Pair.Value)
			continue;
		const FVoxelCoverageRect Rect = Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
											? SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level})
											: MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level});
		if (!CoverageAffects(Rect))
			continue;
		const FGuid Group = BeginPublishGroup(
			{static_cast<uint8>(Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 4 : 5), Pair.Key.Coordinate, Pair.Key.Level});
		const bool bBlocked = BlockedHeightfieldNodes.Contains(Pair.Key) || HandoffPending(Rect);
		if (bBlocked)
		{
			HeightfieldToCommit.Remove(Pair.Key);
			if (Publisher->HasPresentation(Pair.Value))
			{
				co_await UpdatePublicationCoverageSteps(InSources, Group, Pair.Value, {}, CommonTransitionBoxes, true);
				continue;
			}
		}
		co_await UpdatePublicationCoverageSteps(InSources, Group, Pair.Value, {}, CommonTransitionBoxes);
		Publisher->SetHidden(Group, Pair.Value, !DesiredTransitionSignatures.Contains(Pair.Key) || bBlocked);
	}
}

FVoxelGameThreadContinuation FVoxelViewManager::UpdatePublicationCoverageSteps(const FPublicationSources& InSources, const FGuid& InGroup,
	FVoxelViewSourceId InSource, TArray<FBox> InBoxes, TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommon, const bool bInRetainPresented)
{
	bool bSuccess = false;
	if (InSources.bScoped)
		co_await Publisher->SetScopedCoverageSteps(InGroup, InSource, MoveTemp(InBoxes), MoveTemp(InCommon), InSources.ScopeBoxes, bSuccess, bInRetainPresented);
	else if (bInRetainPresented)
		co_await Publisher->SetSharedCoverageSteps(InGroup, InSource, MoveTemp(InCommon), bSuccess);
	else
		co_await Publisher->SetCoverageSteps(InGroup, InSource, MoveTemp(InBoxes), MoveTemp(InCommon), bSuccess);
	if (!bSuccess) MarkPublishGroupCoverageDirty(InGroup);
}
