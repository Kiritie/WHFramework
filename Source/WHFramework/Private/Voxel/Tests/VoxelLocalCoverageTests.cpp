#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Rendering/DWVolumeTransitionPlanner.h"
#include "Voxel/Rendering/DWHeightfieldTransitionBuilder.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Streaming/VoxelInterestRuntime.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCoverageRegionHaloTest,
	"WHFramework.Voxel.Rendering.CoverageRegionHalo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCoverageRegionHaloTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	for (const int32 Offset : {0, -64})
	{
		const FVoxelViewKey Owner{FIntVector(31 + Offset / 2, 0, 0), 1};
		const FVoxelViewKey Boundary{FIntVector(64 + Offset, 0, 0), 0};
		const FVoxelViewKey Interior{FIntVector(80 + Offset, 0, 0), 0};
		const TSet<FVoxelViewKey> Nodes{Owner, Boundary, Interior};
		TMap<FVoxelViewKey, TSet<FVoxelViewKey>> Regions;
		FVoxelVolumeTransitionPlanner::GatherCoverageRegions(Nodes, Regions);
		FVoxelViewKey Region = Owner;
		while (Region.Level < 6) Region = Region.GetParent();
		const auto& Local = Regions.FindChecked(Region);
		TestTrue(TEXT("Local closure retains the owned node and the adjacent fine face"), Local.Contains(Owner) && Local.Contains(Boundary));
		TestFalse(TEXT("Neighbor interior does not invalidate local topology"), Local.Contains(Interior));
		TArray<FVoxelVolumeTransitionFace> GlobalFaces;
		TArray<FVoxelVolumeTransitionFace> LocalFaces;
		FVoxelVolumeTransitionPlanner::Build(Nodes, 6, GlobalFaces);
		FVoxelVolumeTransitionPlanner::Build(Local, 6, LocalFaces);
		TestEqual(TEXT("Thin halo produces every global mixed LOD face"), LocalFaces.Num(), GlobalFaces.Num());
		TestEqual(TEXT("Fixture crosses a region boundary"), GlobalFaces.Num(), 1);
		for (int32 Index = 0; Index < FMath::Min(GlobalFaces.Num(), LocalFaces.Num()); ++Index)
		{
			TestTrue(TEXT("Local seam retains owner, neighbor and direction"),
				LocalFaces[Index].Owner == GlobalFaces[Index].Owner && LocalFaces[Index].Neighbor == GlobalFaces[Index].Neighbor &&
				LocalFaces[Index].Direction == GlobalFaces[Index].Direction);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelManagerLocalCoverageTest,
	"WHFramework.Voxel.Rendering.ManagerLocalCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelManagerLocalCoverageTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler;
	FVoxelTaskBudget Budget;
	Budget.MaxConcurrentTasks = 2;
	Budget.MaxCompletedResultsPerFrame = 1;
	Scheduler.SetBudget(Budget);
	FVoxelViewManager Manager(*Module, Scheduler, 7);
	const TSet<FVoxelViewKey> Nodes{{FIntVector(0, 0, 0), 0}, {FIntVector(1000, 0, 0), 0}};
	Manager.RebuildVolumeTransitions(Nodes);
	TestEqual(TEXT("Distant regions have independent coverage jobs"), Manager.VolumeCoveragePlans.Num(), 2);
	int32 Ready = 0;
	const double Deadline = FPlatformTime::Seconds() + 10.0;
	while (Ready == 0 && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		Ready = 0;
		for (const auto& Pair : Manager.VolumeCoveragePlans) Ready += Pair.Value.Plan.IsValid() ? 1 : 0;
		if (Ready == 0) FPlatformProcess::Sleep(0.001f);
	}
	if (!TestEqual(TEXT("One completion can be applied while the other remains pending"), Ready, 1))
	{
		Scheduler.StopAndJoin();
		return false;
	}
	Manager.RebuildVolumeTransitions(Nodes);
	for (const auto& Pair : Manager.VolumeCoveragePlans)
	{
		const FVoxelGenerationBounds Bounds = Pair.Key.GetBounds();
		const FVoxelCoverageBox Box{Bounds.Min, Bounds.Max};
		TestFalse(TEXT("Uniform Fine ownership does not wait on a redundant LOD plan"),
			VoxelCoverage::IntersectsAny3D(Box, Manager.PendingVolumeHandoffBoxes));
	}
	while (Scheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	Manager.RebuildVolumeTransitions(Nodes);
	TestTrue(TEXT("Ready regions release all local handoff waits"), Manager.PendingVolumeHandoffBoxes.IsEmpty());
	TMap<FVoxelViewKey, TSet<FVoxelViewKey>> Before, After;
	FVoxelVolumeTransitionPlanner::GatherCoverageRegions(Nodes, Before);
	TSet<FVoxelViewKey> Changed = Nodes;
	Changed.Add({FIntVector(1, 0, 0), 0});
	FVoxelVolumeTransitionPlanner::GatherCoverageRegions(Changed, After);
	const FVoxelViewKey Distant{FIntVector(15, 0, 0), 6};
	TestTrue(TEXT("A local edit preserves distant planning inputs"),
		Before.Contains(Distant) && After.Contains(Distant) && Before[Distant].Includes(After[Distant]) && Before[Distant].Num() == After[Distant].Num());
	const FVoxelViewKey LocalRegion{FIntVector::ZeroValue, 6};
	const auto InitialPlan = Manager.VolumeCoveragePlans.FindChecked(LocalRegion).Plan;
	Changed = Nodes;
	Changed.Add({FIntVector(8, 0, 0), 0});
	Manager.RebuildVolumeTransitions(Changed);
	const uint64 PendingSignature = Manager.VolumeCoveragePlans.FindChecked(LocalRegion).PendingSignature;
	Changed.Add({FIntVector(9, 0, 0), 0});
	Manager.RebuildVolumeTransitions(Changed);
	TestTrue(TEXT("Changing topology retains its in-flight immutable plan"), PendingSignature != 0 &&
		Manager.VolumeCoveragePlans.FindChecked(LocalRegion).PendingSignature == PendingSignature);
	TestFalse(TEXT("Unchanged local Fine remains independent of topology changes"),
		VoxelCoverage::IntersectsAny3D(Manager.FineCoverageBox(FIntVector::ZeroValue), Manager.PendingVolumeHandoffBoxes));
	while (Scheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestTrue(TEXT("Completed immutable input is accepted after desired topology changes"),
		Manager.VolumeCoveragePlans.FindChecked(LocalRegion).Plan != InitialPlan);
	Manager.RebuildVolumeTransitions(Changed);
	TestFalse(TEXT("Completed inputs do not block the unchanged local Fine"),
		VoxelCoverage::IntersectsAny3D(Manager.FineCoverageBox(FIntVector::ZeroValue), Manager.PendingVolumeHandoffBoxes));
	while (Scheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	Manager.RebuildVolumeTransitions(Changed);
	TestTrue(TEXT("Coalesced latest topology eventually releases all handoffs"), Manager.PendingVolumeHandoffBoxes.IsEmpty());
	Changed.Add({FIntVector(0, 0, 16), 0});
	Manager.RebuildVolumeTransitions(Changed);
	TestFalse(TEXT("A topology change at another height cannot block ground Fine at the same XY"),
		VoxelCoverage::IntersectsAny3D(Manager.FineCoverageBox(FIntVector::ZeroValue), Manager.PendingVolumeHandoffBoxes));
	while (Scheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	const TSet<FVoxelViewKey> Column{{FIntVector(0, 0, 0), 1}, {FIntVector(0, 0, 1), 1}};
	for (const auto& Key : Column)
	{
		auto Data = MakeShared<FVoxelVoxelProxyData>();
		Data->Key = Key;
		Data->Revision = 1;
		Manager.VoxelProxyData.Add(Key, Data);
	}
	Manager.RebuildVolumeTransitions(Column);
	while (Scheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	Manager.RebuildVolumeTransitions(Column);
	TestEqual(TEXT("Same-column owners can all prepare before any publication"), Manager.DesiredVolumeSignatures.Num(), 2);
	const FVoxelViewKey Prepared{FIntVector(0, 0, 0), 1};
	Manager.PreparedVolumeSignatures.Add(Prepared, Manager.DesiredVolumeSignatures.FindRef(Prepared));
	Manager.RebuildVolumeTransitions(Column);
	TestTrue(TEXT("Another unpublished owner cannot discard a prepared column result"), Manager.PreparedVolumeSignatures.Contains(Prepared));
	Scheduler.StopAndJoin();
	Manager.Reset();
	const FVoxelPublishGroupKey ColumnOwner{0, FIntPoint(-7, 5), 0};
	const FVoxelPublishGroupKey DistantOwner{0, FIntPoint(1000, 5), 0};
	const FVoxelViewKey Original{FIntVector(-7, 5, 0), 0};
	const FVoxelViewKey Intermediate{FIntVector(-7, 5, 1), 0};
	const FVoxelViewKey Latest{FIntVector(-7, 5, 2), 0};
	const FVoxelViewKey DistantNode{FIntVector(1000, 5, 0), 0};
	const TArray<FVoxelViewKey> OlderSnapshot{Intermediate}, NewerSnapshot{Latest};
	Manager.CommitTerrainOwnership(ColumnOwner, MakeArrayView(&Original, 1));
	Manager.CommitTerrainOwnership(DistantOwner, MakeArrayView(&DistantNode, 1));
	Manager.CommitTerrainOwnership(ColumnOwner, OlderSnapshot);
	Manager.CommitTerrainOwnership(ColumnOwner, NewerSnapshot);
	TestTrue(TEXT("A later immutable ownership snapshot replaces nodes committed by its predecessor"),
		Manager.VisibleTerrainNodes.Contains(Latest) && !Manager.VisibleTerrainNodes.Contains(Original) &&
		!Manager.VisibleTerrainNodes.Contains(Intermediate));
	TestTrue(TEXT("Replacing a column preserves an independent distant owner"), Manager.VisibleTerrainNodes.Contains(DistantNode));
	Manager.CommitTerrainOwnership(ColumnOwner, {});
	TestFalse(TEXT("An empty owner target retires its latest committed vertical nodes"), Manager.VisibleTerrainNodes.Contains(Latest));
	TestFalse(TEXT("An empty target releases its derived owner lookup"), Manager.VisibleTerrainNodesByGroup.Contains(ColumnOwner));
	Manager.Reset();
	TestTrue(TEXT("Reset releases both committed nodes and their owner index"),
		Manager.VisibleTerrainNodes.IsEmpty() && Manager.VisibleTerrainNodesByGroup.IsEmpty());
	Manager.bCoverageDirty = false;
	Manager.bCoverageDirtyFull = false;
	FVoxelInterestDelta DataOnly;
	DataOnly.AddedExact.Add(FIntVector::ZeroValue);
	Manager.ApplyInterestDelta(DataOnly);
	TestFalse(TEXT("Data or priority revisions do not force visual coverage work"), Manager.bCoverageDirty);
	FVoxelInterestDelta VisualDelta;
	VisualDelta.bVisualPartitionChanged = true;
	VisualDelta.AddedFine.Add(FIntVector(-2, -2, 0));
	VisualDelta.RemovedFine.Add(FIntVector(-2, -2, 1));
	VisualDelta.AddedProxy.Add({FIntVector(1000, 0, 0), 1});
	VisualDelta.RemovedSurface.Add({FIntPoint(4000, 0), 0});
	VisualDelta.AddedMacro.Add({FIntPoint(100, 0), 0});
	Manager.ApplyInterestDelta(VisualDelta);
	TestTrue(TEXT("A visual delta dirties only its added and removed representation footprints"),
		Manager.bCoverageDirty && !Manager.bCoverageDirtyFull && Manager.DirtyCoverageRects.Num() == 4);
	Manager.bActiveCoverageFull = false;
	Manager.ActiveDirtyCoverageRects = Manager.DirtyCoverageRects;
	TestTrue(TEXT("Negative Fine additions and vertical removals share the same local scope"),
		Manager.CoverageAffects(Manager.FineCoverageRect(FIntVector(-2, -2, 0))));
	TestTrue(TEXT("A removed distant Surface is included in the coverage handoff"),
		Manager.CoverageAffects(Manager.SurfaceCoverageRect({FIntPoint(4000, 0), 0})));
	TestFalse(TEXT("Independent space between sparse changes is not refreshed"),
		Manager.CoverageAffects(Manager.FineCoverageRect(FIntVector(100, 100, 0))));
	Manager.ActiveDirtyCoverageRects = {{FIntPoint::ZeroValue, FIntPoint(16, 16)}};
	Manager.ExpandActiveCoverage(Manager.VoxelProxyCoverageRect({FIntVector(1000, 0, 0), 1}));
	TestTrue(TEXT("A discovered distant handoff extends the active scope without a global refresh"),
		!Manager.bActiveCoverageFull && Manager.CoverageAffects(Manager.FineCoverageRect(FIntVector::ZeroValue)) &&
		Manager.CoverageAffects(Manager.VoxelProxyCoverageRect({FIntVector(1000, 0, 0), 1})));
	TestFalse(TEXT("Extending a distant handoff preserves unrelated intermediate space"),
		Manager.CoverageAffects(Manager.FineCoverageRect(FIntVector(100, 100, 0))));
	Manager.Reset();
	FVoxelTaskScheduler ScopedScheduler;
	FVoxelViewManager ScopedManager(*Module, ScopedScheduler, 8);
	auto& Interest = const_cast<FVoxelInterestSet&>(Module->GetCurrentInterest());
	const FVoxelViewKey NearRoot{FIntVector::ZeroValue, 1};
	const FVoxelViewKey FarRoot{FIntVector(100, 0, 0), 1};
	Interest.TerrainPlan.Roots = {NearRoot, FarRoot};
	Interest.TerrainPlan.Required = Interest.TerrainPlan.Roots;
	TArray<FVoxelViewKey> NearChildren;
	NearRoot.GetChildren(NearChildren);
	for (const auto& Child : NearChildren)
	{
		Interest.TerrainPlan.Leaves.Add(Child);
		Interest.TerrainPlan.Required.Add(Child);
		ScopedManager.FineReady.Add(Child.Coordinate);
	}
	Interest.TerrainPlan.Leaves.Add(FarRoot);
	ScopedManager.CommitTerrainOwnership({1, FIntPoint(NearRoot.Coordinate.X, NearRoot.Coordinate.Y), NearRoot.Level}, MakeArrayView(&NearRoot, 1));
	ScopedManager.CommitTerrainOwnership({1, FIntPoint(FarRoot.Coordinate.X, FarRoot.Coordinate.Y), FarRoot.Level}, MakeArrayView(&FarRoot, 1));
	ScopedManager.bActiveCoverageFull = false;
	ScopedManager.ActiveDirtyCoverageRects = {{FIntPoint::ZeroValue, FIntPoint(16, 16)}};
	ScopedManager.ResolveTransitionVisibility();
	ScopedManager.Publisher->Tick();
	TestFalse(TEXT("Local ready children retire their coarse parent"), ScopedManager.VisibleTerrainNodes.Contains(NearRoot));
	for (const auto& Child : NearChildren)
		TestTrue(TEXT("A dirty root publishes its entire ready sibling partition"), ScopedManager.VisibleTerrainNodes.Contains(Child));
	TestTrue(TEXT("A distant unready root retains its previous committed ownership"), ScopedManager.VisibleTerrainNodes.Contains(FarRoot));
	UWorld* FixtureWorld = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(FixtureWorld);
	const FVoxelSurfaceTileKey SurfaceKey{FIntPoint::ZeroValue, 1};
	auto Surface = MakeShared<FVoxelSurfaceTileData>();
	Surface->Key = SurfaceKey;
	Surface->Step = 2;
	Surface->Revision = 1;
	Surface->GroundZ.Init(32, Surface->GetVertexCount());
	Surface->WaterZ.Init(MIN_int32, Surface->GetVertexCount());
	Surface->SurfaceMaterial.Init(0, Surface->GetVertexCount());
	Surface->Flags.Init(0, Surface->GetVertexCount());
	ScopedManager.SurfaceData.Add(SurfaceKey, Surface);
	ScopedManager.SurfaceReady.Add(SurfaceKey);
	ScopedManager.SurfaceWanted.Add(SurfaceKey);
	ScopedManager.SurfaceActors.Add(SurfaceKey, FixtureWorld->SpawnActor<AActor>());
	ScopedManager.VisibleTerrainNodes.Reset();
	ScopedManager.VisibleTerrainNodesByGroup.Reset();
	ScopedManager.CommitTerrainOwnership({1, FIntPoint(NearRoot.Coordinate.X, NearRoot.Coordinate.Y), NearRoot.Level}, MakeArrayView(&NearRoot, 1));
	ScopedManager.CommitTerrainOwnership({1, FIntPoint(FarRoot.Coordinate.X, FarRoot.Coordinate.Y), FarRoot.Level}, MakeArrayView(&FarRoot, 1));
	ScopedManager.bActiveCoverageFull = false;
	ScopedManager.ActiveDirtyCoverageRects = {{FIntPoint::ZeroValue, FIntPoint(16, 16)}};
	ScopedManager.ResolveTransitionVisibility();
	ScopedManager.Publisher->Tick();
	TestTrue(TEXT("Fixture has a pending heightfield topology plan"), !ScopedManager.PendingHeightfieldPlanOwners.IsEmpty());
	for (const auto& Child : NearChildren)
		TestTrue(TEXT("Ready Fine ownership does not wait on its enclosing heightfield topology"), ScopedManager.VisibleTerrainNodes.Contains(Child));
	const FVoxelHeightfieldNodeKey SurfaceOwner{EVoxelHeightfieldRepresentation::Surface, SurfaceKey.Coordinate, SurfaceKey.Level};
	const uint64 FirstPending = ScopedManager.HeightfieldCoveragePlans.FindChecked(SurfaceOwner).PendingSignature;
	auto RevisedSurface = MakeShared<FVoxelSurfaceTileData>(*Surface);
	RevisedSurface->Revision = 2;
	ScopedManager.SurfaceData.Add(SurfaceKey, RevisedSurface);
	ScopedManager.RebuildHeightfieldTransitions();
	const auto& PendingState = ScopedManager.HeightfieldCoveragePlans.FindChecked(SurfaceOwner);
	TestTrue(TEXT("Heightfield changes coalesce without canceling the in-flight immutable plan"),
		FirstPending != 0 && PendingState.PendingSignature == FirstPending && PendingState.DesiredSignature != FirstPending);
	const double HeightfieldDeadline = FPlatformTime::Seconds() + 10.0;
	while (ScopedScheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < HeightfieldDeadline)
	{
		ScopedScheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestTrue(TEXT("An immutable heightfield plan completes despite a newer desired revision"),
		ScopedManager.HeightfieldCoveragePlans.FindChecked(SurfaceOwner).Plan.IsValid());
	ScopedManager.RebuildHeightfieldTransitions();
	while (ScopedScheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < HeightfieldDeadline)
	{
		ScopedScheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	ScopedManager.RebuildHeightfieldTransitions();
	TestTrue(TEXT("The coalesced heightfield revision eventually releases its topology wait"), ScopedManager.PendingHeightfieldPlanOwners.IsEmpty());
	ScopedScheduler.StopAndJoin();
	ScopedManager.Reset();
	FixtureWorld->DestroyWorld(false);
	GEngine->DestroyWorldContext(FixtureWorld);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHeightfieldLocalDependenciesTest,
	"WHFramework.Voxel.Rendering.HeightfieldLocalDependencies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHeightfieldLocalDependenciesTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	TArray<int32> Ground, Water;
	TArray<uint16> Materials;
	Ground.Init(32, 33 * 33);
	Water.Init(MIN_int32, 33 * 33);
	Materials.Init(0, 33 * 33);
	TArray<FVoxelHeightfieldTileView> Views;
	const auto AddView = [&](FIntPoint Coordinate, FIntPoint Origin, int32 Step)
	{
		auto& View = Views.AddDefaulted_GetRef();
		View.Key = {EVoxelHeightfieldRepresentation::Surface, Coordinate, static_cast<uint8>(Step == 2 ? 1 : 0)};
		View.Origin = Origin;
		View.Side = 33;
		View.Step = Step;
		View.Revision = 1;
		View.Ground = Ground;
		View.Water = Water;
		View.WaterKind.Init(0, 33 * 33);
		View.Material = Materials;
	};
	AddView({-1, 0}, {-32, 0}, 1);
	AddView({0, 0}, {0, 0}, 2);
	AddView({-1, 1}, {-32, 32}, 1);
	AddView({1000, 0}, {32000, 0}, 1);
	const FVoxelHeightfieldNodeKey PendingKey{EVoxelHeightfieldRepresentation::Surface, {2, 0}, 0};
	const TArray<FVoxelHeightfieldTileFootprint> Pending{{PendingKey, {64, 0}, 32, 1}};
	TMap<FVoxelHeightfieldNodeKey, TArray<FVoxelHeightfieldNodeKey>> Dependencies;
	FVoxelHeightfieldTransitionBuilder::GatherOwnerDependencies(Views, Pending, Dependencies);
	TestTrue(TEXT("A coarse owner retains its adjacent pending tile"), Dependencies[Views[1].Key].Contains(PendingKey));
	TestEqual(TEXT("Distant owner remains independent of local waits"), Dependencies[Views[3].Key].Num(), 1);
	TArray<FVoxelHeightfieldTransitionEdge> Edges;
	FString Error;
	if (!TestTrue(TEXT("The full heightfield transition reference builds"),
		FVoxelHeightfieldTransitionBuilder::BuildEdges(Views, Edges, Error))) return false;
	TestTrue(TEXT("Fixture contains mixed LOD seams"), !Edges.IsEmpty());
	for (const auto& Edge : Edges)
	{
		TestTrue(TEXT("Local dependency closure includes every full-plan seam"), Dependencies[Edge.Owner].Contains(Edge.Neighbor));
	}
	TMap<FVoxelHeightfieldNodeKey, TArray<FVoxelHeightfieldNodeKey>> Reordered;
	Algo::Reverse(Views);
	FVoxelHeightfieldTransitionBuilder::GatherOwnerDependencies(Views, Pending, Reordered);
	for (const auto& Pair : Dependencies)
	{
		TestTrue(TEXT("Dependency order is independent of input loading order"), Reordered[Pair.Key] == Pair.Value);
	}
	return true;
}

#endif
