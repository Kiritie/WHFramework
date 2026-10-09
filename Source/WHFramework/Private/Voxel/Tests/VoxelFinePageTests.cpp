#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Rendering/VoxelFinePage.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Rendering/VoxelTerrainStagePlanner.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Runtime/VoxelChangeHierarchy.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Tests/VoxelTestUtilities.h"
#include "Voxel/Streaming/VoxelInterestManager.h"
#include "Voxel/Streaming/VoxelInterestRuntime.h"
#include "Voxel/VoxelModule.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFinePageLayoutTest,
	"WHFramework.Voxel.Rendering.FinePageLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFinePageLayoutTest::RunTest(const FString& InParameters)
{
	FString PlanningError;
	FVoxelViewSettings Settings;
	Settings.Lod.MaximumFineLevel = 4;
	FVoxelStreamingSource Source;
	Source.Center = FIntVector(8);
	Source.Capabilities = EVoxelStreamingCapability::FineVisual;
	const auto Sources = MakeArrayView(&Source, 1);
	TestEqual(TEXT("Near band is F0"), FVoxelFinePagePlanner::ResolveDesiredLevel({1, 0, 0}, Sources, Settings), uint8(0));
	TestEqual(TEXT("32 cells begins F1"), FVoxelFinePagePlanner::ResolveDesiredLevel({3, 0, 0}, Sources, Settings), uint8(1));
	TestEqual(TEXT("64 cells begins F2"), FVoxelFinePagePlanner::ResolveDesiredLevel({5, 0, 0}, Sources, Settings), uint8(2));
	const FVoxelFinePageKey Parent{{2, 0, 0}, 2};
	TArray<FIntVector> Sections;
	FVoxelFinePagePlanner::GatherSections(Parent, Sections);
	TSet<FIntVector> Wanted;
	Wanted.Append(Sections);
	Source.Center = FIntVector(-128, -128, 8);
	Source.View.FineRadiusCells = 1024;
	FVoxelFinePagePlan Complete;
	FVoxelFinePagePlanner::Build(Wanted, Sources, Settings, Complete, PlanningError);
	TestTrue(TEXT("A complete distant family merges bottom up"), Complete.Pages.Contains(Parent));
	TSet<FIntVector> BeforeSections;
	for (const auto& Pair : Complete.SectionOwners) BeforeSections.Add(Pair.Key);
	TestTrue(TEXT("Page layout covers the exact logical Fine demand"),
		BeforeSections.Num() == Wanted.Num() && BeforeSections.Includes(Wanted));
	Source.Center = Parent.GetBounds().Min + FIntVector(8);
	FVoxelFinePagePlan NearLayout;
	FVoxelFinePagePlanner::Build(Wanted, Sources, Settings, NearLayout, PlanningError);
	TestFalse(TEXT("Moving into the near band splits the presentation Page"), NearLayout.Pages.Contains(Parent));
	TSet<FIntVector> AfterSections;
	for (const auto& Pair : NearLayout.SectionOwners) AfterSections.Add(Pair.Key);
	TestTrue(TEXT("The same Fine Sections survive presentation regrouping"),
		BeforeSections.Num() == AfterSections.Num() && BeforeSections.Includes(AfterSections));
	Source.Center = FIntVector(-128, -128, 8);
	Wanted.Remove(Sections.Last());
	FVoxelFinePagePlan Incomplete;
	FVoxelFinePagePlanner::Build(Wanted, Sources, Settings, Incomplete, PlanningError);
	TestFalse(TEXT("One missing Section prevents the F2 owner"), Incomplete.Pages.Contains(Parent));
	TestEqual(TEXT("Boundary split retains every remaining Section exactly once"), Incomplete.SectionOwners.Num(), Wanted.Num());
	for (const FVoxelFinePageKey& Page : Incomplete.Pages)
	{
		FVoxelFinePagePlanner::GatherSections(Page, Sections);
		for (const FIntVector& Section : Sections) TestTrue(TEXT("Every Page has a complete family"), Wanted.Contains(Section));
	}
	Source.Center = FIntVector(8);
	FVoxelWorldManifest Manifest;
	FVoxelInterestSet Raw;
	Source.View.FineRadiusCells = 80;
	Source.Capabilities |= EVoxelStreamingCapability::Data;
	FVoxelInterestManager().BuildSourceExact(Source, Manifest, Settings, Raw);
	Wanted.Reset();
	for (const auto& Pair : Raw.Exact) if (Pair.Value.bFineRender) Wanted.Add(Pair.Key);
	FVoxelFinePagePlanner::Build(Wanted, Sources, Settings, Complete, PlanningError);
	for (const auto& Page : Complete.Pages) TestTrue(TEXT("MaximumFineLevel does not force F3/F4 in an 80 cell domain"), Page.Level <= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFinePageMeshTest,
	"WHFramework.Voxel.Rendering.FinePageMeshLossless",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFinePageMeshTest::RunTest(const FString& InParameters)
{
	FString PlanningError;
	FVoxelSectionMeshResult Section;
	for (const uint8 Variant : {uint8(0), uint8(1)})
	{
		auto& Batch = Section.Batches.AddDefaulted_GetRef();
		Batch.Bank = 2;
		Batch.MaterialVariant = Variant;
		Batch.Mesh.Vertices = {FVector(0, 0, 0), FVector(1, 0, 0), FVector(0, 1, 0)};
		Batch.Mesh.Triangles = {0, 1, 2};
		Batch.Mesh.Normals.Init(FVector::UpVector, 3);
		Batch.Mesh.Tangents.Init(FProcMeshTangent(1, 0, 0), 3);
		Batch.Mesh.UV0 = {FVector2D(0, 0), FVector2D(1, 0), FVector2D(0, 1)};
		Batch.Mesh.UV1.Init(FVector2D(2, 3), 3);
		Batch.Mesh.UV2.Init(FVector2D(4, 5), 3);
		Batch.Mesh.Colors.Init(FLinearColor::Red, 3);
	}
	for (uint8 Level = 0; Level <= 3; ++Level)
	{
		FVoxelSectionMeshResult Combined;
		TArray<FIntVector> Sections;
		FVoxelFinePagePlanner::GatherSections({FIntVector(-1, 0, -1), Level}, Sections);
		const FIntVector Origin = Sections[0] * 16;
		for (const FIntVector& Key : Sections) FVoxelFinePagePlanner::AppendMesh(Section, FVector(Key * 16 - Origin), Combined);
		TestEqual(TEXT("Material variants remain separate"), Combined.Batches.Num(), 2);
		for (int32 BatchIndex = 0; BatchIndex < Combined.Batches.Num(); ++BatchIndex)
		{
			const auto& Mesh = Combined.Batches[BatchIndex].Mesh;
			TestEqual(TEXT("Fine vertices are concatenated without resampling"), Mesh.Vertices.Num(), Sections.Num() * 3);
			TestTrue(TEXT("Combined buffers and triangle indices validate"), Mesh.Validate());
			for (int32 Index = 0; Index < Mesh.Vertices.Num(); ++Index)
			{
				const auto& Original = Section.Batches[BatchIndex].Mesh;
				TestTrue(TEXT("Every vertex retains its cell scale and section offset"),
					Mesh.Vertices[Index] == Original.Vertices[Index % 3] + FVector(Sections[Index / 3] * 16 - Origin));
				TestTrue(TEXT("Fine texture, material and color attributes survive merging"),
					Mesh.UV0[Index] == Original.UV0[Index % 3] && Mesh.UV1[Index] == Original.UV1[Index % 3] &&
					Mesh.UV2[Index] == Original.UV2[Index % 3] && Mesh.Colors[Index] == Original.Colors[Index % 3] &&
					Mesh.Normals[Index] == Original.Normals[Index % 3]);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelLodPolicyTest,
	"WHFramework.Voxel.Streaming.LodPolicyInvariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelLodPolicyTest::RunTest(const FString& InParameters)
{
	(void)InParameters;

	FString PlanningError;
	FVoxelStreamingSource Source;
	FVoxelViewSettings Settings;
	Source.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::FineVisual | EVoxelStreamingCapability::WorldVisual;

	FVoxelSourceInterest Accepted;
	Accepted.FirstSideCells[0] = 64;
	Accepted.FirstSideCells[1] = 256;
	Accepted.FirstSideCells[2] = 2048;

	const auto Steps = VoxelViewLod::ResolveReplanSteps(Source, Settings, &Accepted);

	TestEqual(TEXT("Fine replan is one Section"), Steps.Fine, 16);
	TestEqual(TEXT("Proxy replan follows its own radius"), Steps.Proxy, 64);
	TestEqual(TEXT("Surface replan uses nearest power of two"), Steps.Surface, 256);
	TestEqual(TEXT("Macro replan no longer inherits accepted page side"), Steps.Macro, 1024);

	Source.View.VoxelProxyRadiusCells *= 2;
	TestEqual(TEXT("Changing radius automatically changes Proxy replan"), VoxelViewLod::ResolveReplanSteps(Source, Settings, &Accepted).Proxy, 128);
	Source.View.VoxelProxyRadiusCells /= 2;

	int32 NearStep = 0;
	int32 FarStep = 0;
	bool bResolutionFloor = false;
	TestTrue(TEXT("Near Proxy sampling resolves independently of spatial level"),
		VoxelStreamingGeometry::ResolveSampleStep(256, Source.VerticalFovDegrees, Source.ViewportHeightPixels,
			Settings.Lod.TargetScreenErrorPixels, 1024, NearStep, bResolutionFloor, PlanningError, 2));
	TestTrue(TEXT("Distant Proxy sampling resolves within its actual geometry bounds"),
		VoxelStreamingGeometry::ResolveSampleStep(100000, Source.VerticalFovDegrees, Source.ViewportHeightPixels,
			Settings.Lod.TargetScreenErrorPixels, 1024, FarStep, bResolutionFloor, PlanningError, 2));
	TestTrue(TEXT("Increasing distance permits a coarser actual sample"), NearStep >= 2 && FarStep > NearStep);

	FVoxelWorldManifest Manifest;
	FVoxelInterestManager Manager;
	const auto Sources = MakeArrayView(&Source, 1);
	const auto Baseline = Manager.Compute(Sources, Manifest, Settings);

	Settings.Budget.FineAdmissionPerFrame = 1;
	Settings.Budget.VoxelProxyAdmissionPerFrame = 1;
	Settings.Budget.MaxConcurrentSurfaceTasks = 1;
	const auto Slow = Manager.Compute(Sources, Manifest, Settings);

	TestTrue(TEXT("Throughput cannot change actual volume pages"),
		Baseline.VoxelProxy.Num() == Slow.VoxelProxy.Num() && Baseline.VoxelProxy.Includes(Slow.VoxelProxy));
	TestTrue(TEXT("Throughput cannot change heightfield topology"), Baseline.Surface.Num() == Slow.Surface.Num() && Baseline.Surface.Includes(Slow.Surface) &&
																		Baseline.Macro.Num() == Slow.Macro.Num() && Baseline.Macro.Includes(Slow.Macro));

	FVoxelInterestSet ProxyA;
	FVoxelInterestSet ProxyB;
	FVoxelInterestSet FineA;
	FVoxelInterestSet FineB;
	Manager.BuildSourceProxy(Source, Manifest, Settings, ProxyA);
	Manager.BuildSourceExact(Source, Manifest, Settings, FineA);

	Source.View.FineVerticalHalfExtentCells += 64;
	Manager.BuildSourceProxy(Source, Manifest, Settings, ProxyB);
	TestTrue(TEXT("Fine vertical extent does not change Proxy source demand"),
		ProxyA.VoxelProxy.Num() == ProxyB.VoxelProxy.Num() && ProxyA.VoxelProxy.Includes(ProxyB.VoxelProxy));

	Source.View.FineVerticalHalfExtentCells -= 64;
	Source.View.VoxelProxyVerticalHalfExtentCells += 128;
	Manager.BuildSourceExact(Source, Manifest, Settings, FineB);
	TestTrue(TEXT("Proxy vertical extent does not change Fine source demand"), FineA.Exact.Num() == FineB.Exact.Num());
	Source.View.VoxelProxyVerticalHalfExtentCells -= 128;

	Source.VelocityCellsPerSecond = FVector(512, 0, 0);
	const auto Moving = Manager.Compute(Sources, Manifest, Settings);
	TestTrue(TEXT("Forward prefetch adds data beyond the stationary set"), Moving.Exact.Num() > Slow.Exact.Num());
	TestTrue(TEXT("Forward prefetch does not expand the Fine display domain"),
		Moving.FineSections->Num() == Slow.FineSections->Num() && Moving.FineSections->Includes(*Slow.FineSections));

	FVoxelTerrainStageSource StageSource{Source, 128, 32, 0};
	FVoxelTerrainStagePlan Plan;
	FString Error;
	if (!TestTrue(TEXT("Actual coarse planner provides the retained baseline"),
		FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::VoxelProxy, MakeArrayView(&StageSource, 1),
			3, 64, -64, 64, 8.0, 17, 23, Plan, Error))) return false;
	const auto PreviousPages = Plan.Pages;
	TestFalse(TEXT("Invalid topology explicitly fails"),
		FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::VoxelProxy, MakeArrayView(&StageSource, 1),
			21, 64, -64, 64, 8.0, 17, 23, Plan, Error));
	TestTrue(TEXT("Failure preserves the previous valid geometry plan"), !Error.IsEmpty() && Plan.Pages.Num() == PreviousPages.Num());
	for (const auto& Pair : PreviousPages)
	{
		const FVoxelTerrainGeometry* Retained = Plan.Pages.Find(Pair.Key);
		TestTrue(TEXT("Rejected policy retains each exact previous geometry"), Retained && *Retained == Pair.Value);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFinePageCoverageTest,
	"WHFramework.Voxel.Rendering.FinePageCoverageInvariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFinePageCoverageTest::RunTest(const FString& InParameters)
{
	FString PlanningError;
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler(true);
	Module->Runtime = MakeUnique<FVoxelWorldRuntime>(1, true, VoxelTest::MakeRegistry(),
		VoxelTest::MakeGenerator(), Scheduler.GetTerrainResources());
	FVoxelViewManager Manager(*Module, Scheduler, 1);
	Manager.bFinePresentationDirty = false;
	Manager.bHeightfieldTopologyDirty = false;
	Manager.ResolveTransitionVisibility();
	TestEqual(TEXT("One coverage pass constructs one immutable snapshot"), Manager.CoverageSnapshotBuildCount, uint64(1));
	TestEqual(TEXT("Fine-only work does not rebuild heightfield topology"), Manager.HeightfieldTopologyRebuildCount, uint64(0));
	{
		FVoxelViewManager MovingManager(*Module, Scheduler, 1);
		MovingManager.bHeightfieldTopologyDirty = false;
		const FVoxelFinePageKey Parent{FIntVector::ZeroValue, 2};
		TArray<FIntVector> FixedSections;
		FVoxelFinePagePlanner::GatherSections(Parent, FixedSections);
		for (const FIntVector& Section : FixedSections) MovingManager.GetInputRoot().FineWanted.Add(Section);
		auto& Interest = const_cast<FVoxelInterestSet&>(Module->GetCurrentInterest());
		FVoxelSourceInterest SourceInterest;
		SourceInterest.Source.Capabilities = EVoxelStreamingCapability::FineVisual;
		SourceInterest.Source.Center = FIntVector(-128, -128, 8);
		SourceInterest.Source.View.FineRadiusCells = 512;
		const FGuid SourceId = FGuid::NewGuid();
		Interest.Sources.Add(SourceId, SourceInterest);
		MovingManager.RebuildFinePagePlan();
		TestTrue(TEXT("Distant fixed demand initially merges into F2"), MovingManager.GetInputRoot().FinePagePlan.Pages.Contains(Parent));
		const uint64 LayoutRevision = MovingManager.FinePageLayoutRevision;
		Interest.Sources[SourceId].Source.Center = FIntVector(8);
		MovingManager.RefreshTaskPriorities();
		TestTrue(TEXT("Source-only snapshot refreshes Page bands without logical demand changes"),
			MovingManager.FinePageLayoutRevision > LayoutRevision && !MovingManager.GetInputRoot().FinePagePlan.Pages.Contains(Parent));
		TestEqual(TEXT("Source-only Page refresh preserves Fine demand"), MovingManager.GetInputRoot().FineWanted.Num(), FixedSections.Num());
		TestFalse(TEXT("Source-only Page refresh does not dirty heightfield topology"), MovingManager.bHeightfieldTopologyDirty);
		Interest.Sources.Reset();
	}
	{
		auto& Interest = const_cast<FVoxelInterestSet&>(Module->GetCurrentInterest());
		Interest.PublicationInputs = MakeShared<FVoxelTerrainPublicationInputs, ESPMode::ThreadSafe>();
		FVoxelViewManager PendingManager(*Module, Scheduler, 1);
		PendingManager.PendingPublicationInputs = Interest.PublicationInputs;
		PendingManager.PendingInterestRevision = 2;
		PendingManager.InterestInputPhase = 5;
		PendingManager.InterestInputCursor = 137;
		for (int32 Refresh = 0; Refresh < 10; ++Refresh)
		{
			PendingManager.RefreshTaskPriorities();
		}
		TestTrue(TEXT("Priority refresh retains pending publication inputs"), PendingManager.PendingPublicationInputs == Interest.PublicationInputs);
		TestEqual(TEXT("Priority refresh preserves input phase"), PendingManager.InterestInputPhase, 5);
		TestEqual(TEXT("Priority refresh preserves input cursor"), PendingManager.InterestInputCursor, 137);
		PendingManager.AppliedInterestRevision = 2;
		PendingManager.RefreshTaskPriorities();
		TestEqual(TEXT("Priority refresh keeps committed coverage eligible"), PendingManager.AppliedInterestRevision, uint64(2));
		TestTrue(TEXT("Priority refresh is deferred to the frame budget"), PendingManager.bTaskPrioritiesDirty);
		Interest.PublicationInputs.Reset();
	}
	for (const bool bAdd : {true, false})
	{
		for (const bool bSurface : {true, false})
		{
			FVoxelInterestDelta Delta;
			Delta.bVisualPartitionChanged = true;
			if (bSurface) (bAdd ? Delta.AddedSurface : Delta.RemovedSurface).Add({FIntPoint(100, 0), 0});
			else (bAdd ? Delta.AddedMacro : Delta.RemovedMacro).Add({FIntPoint(100, 0), 0});
			Manager.ApplyInterestDelta(Delta);
			TestTrue(TEXT("Surface/Macro membership changes dirty topology"), Manager.bHeightfieldTopologyDirty);
			Manager.ResolveTransitionVisibility();
		}
	}
	TestEqual(TEXT("Each topology change rebuilds once"), Manager.HeightfieldTopologyRebuildCount, uint64(4));
	const auto RevisedProxyCoverage = MakeShared<FVoxelVoxelProxyData, ESPMode::ThreadSafe>();
	RevisedProxyCoverage->Key = {FIntVector(100, 0, 0), 1};
	RevisedProxyCoverage->Geometry.SampleStepCells = 2;
	RevisedProxyCoverage->Cells.Init(FVoxelBlockState(), 16 * 16 * 16);
	bool bSurfaceComplete = false;
	if (!TestTrue(TEXT("Proxy覆盖经过真实逐行采样"), RevisedProxyCoverage->AdvanceSurfaceCoverage(
		16, bSurfaceComplete, PlanningError) && bSurfaceComplete)) return false;
	Manager.UpdateProxySurfaceCoverage(RevisedProxyCoverage);
	TestTrue(TEXT("A Proxy surface coverage revision dirties heightfield topology"), Manager.bHeightfieldTopologyDirty);
	Manager.ResolveTransitionVisibility();
	TestEqual(TEXT("Proxy surface coverage rebuilds heightfield topology once"), Manager.HeightfieldTopologyRebuildCount, uint64(5));
	{
		FVoxelViewManager EditedManager(*Module, Scheduler, 1);
		EditedManager.bCoverageDirtyFull = false;
		EditedManager.bHeightfieldTopologyDirty = false;
		const FVoxelViewKey LocalProxy{FIntVector::ZeroValue, 4};
		const FVoxelSurfaceTileKey DistantSurface{FIntPoint(1000, 1000), 0};
		const FVoxelMacroTileKey DistantMacro{FIntPoint(1000, 1000), 0};
		EditedManager.GetInputRoot().ProxyWanted.Add(LocalProxy);
		EditedManager.VoxelProxyReady.Add(LocalProxy);
		EditedManager.SurfaceReady.Add(DistantSurface);
		EditedManager.MacroReady.Add(DistantMacro);
		FVoxelChangeHierarchy Hierarchy;
		const FVoxelLodTopologySettings Lod;
		Hierarchy.SetTopology(Lod);
		FVoxelTerrainGeometry RegisteredProxy;
		RegisteredProxy.Key = {EVoxelTerrainRepresentation::VoxelProxy, LocalProxy.Coordinate, LocalProxy.Level};
		RegisteredProxy.SampleStepCells = 16;
		if (!TestTrue(TEXT("局部编辑夹具登记实际参与的Proxy空间身份"), Hierarchy.RegisterGeometry(RegisteredProxy, PlanningError))) return false;
		const FVoxelAffectedRepresentationKeys Affected = Hierarchy.GatherAffectedKeys(FIntVector(3, 0, 15));
		for (const auto& Key : Affected.Proxy) TestTrue(TEXT("Edit enumerates only configured Proxy levels"), Key.Level <= Lod.MaximumVoxelProxyLevel);
		for (const auto& Key : Affected.Surface) TestTrue(TEXT("Edit enumerates only configured Surface levels"), Key.Level <= Lod.MaximumSurfaceSpatialLevel);
		for (const auto& Key : Affected.Macro) TestTrue(TEXT("Edit enumerates only configured Macro levels"), Key.Level <= Lod.MaximumMacroSpatialLevel);
		EditedManager.InvalidateRepresentationKeys(Affected);
		TestFalse(TEXT("Affected ready Proxy is invalidated"), EditedManager.VoxelProxyReady.Contains(LocalProxy));
		TestTrue(TEXT("Unrelated ready heightfields survive a local edit"), EditedManager.SurfaceReady.Contains(DistantSurface) && EditedManager.MacroReady.Contains(DistantMacro));
		TestEqual(TEXT("Unused ancestors cannot expand an edit to the entire display domain"), EditedManager.DirtyCoverageRects.Num(), 1);
		if (!EditedManager.DirtyCoverageRects.IsEmpty())
		{
			TestTrue(TEXT("Only the participating 256-cell Proxy and existing one-cell coverage halo are dirty"), EditedManager.DirtyCoverageRects[0].Min == FIntPoint(-1) && EditedManager.DirtyCoverageRects[0].Max == FIntPoint(257));
		}
		TestFalse(TEXT("An edit with no affected ready heightfield does not dirty its topology"), EditedManager.bHeightfieldTopologyDirty);
	}
	const FVoxelFinePageKey Old{{10, 0, 0}, 1};
	TArray<FIntVector> Sections;
	FVoxelFinePagePlanner::GatherSections(Old, Sections);
	for (const auto& Section : Sections)
	{
		Manager.PresentedFineSectionOwners.Add(Section, Old);
		const FVoxelFinePageKey Desired{Section, 0};
		Manager.GetInputRoot().FinePagePlan.SectionOwners.Add(Section, Desired);
		Manager.GetInputRoot().FinePagePlan.Pages.Add(Desired);
		Manager.GetInputRoot().FineWanted.Add(Section);
	}
	for (int32 Index = 0; Index < 1000; ++Index)
	{
		const FIntVector Distant(10000 + Index, 0, 0);
		const FVoxelFinePageKey Page{Distant, 0};
		Manager.GetInputRoot().FinePagePlan.SectionOwners.Add(Distant, Page);
		Manager.GetInputRoot().FinePagePlan.Pages.Add(Page);
	}
	TSet<FVoxelFinePageKey> Candidates;
	Manager.bActiveCoverageFull = false;
	const auto Bounds = Old.GetBounds();
	Manager.ActiveDirtyCoverageRects = {{FIntPoint(Bounds.Min.X, Bounds.Min.Y), FIntPoint(Bounds.Max.X, Bounds.Max.Y)}};
	const FVoxelFinePageKey DistantDirty{FIntVector(10000, 0, 0), 0};
	Manager.DirtyFinePages.Add(DistantDirty);
	Manager.GatherFinePageCandidates({{Sections[0], 0}}, Candidates);
	TestEqual(TEXT("Local handoff includes one old Page and its eight replacements only"), Candidates.Num(), 9);
	TestFalse(TEXT("A distant pending edit does not expand this pass's candidates"), Candidates.Contains(DistantDirty));
	Manager.DirtyFinePages.Remove(DistantDirty);
	TestEqual(TEXT("Local lookup never traverses the terrain actor maps"), Manager.FullTerrainActorScanCount, uint64(0));
	Manager.InvalidateFinePresentation(Sections[0]);
	TestEqual(TEXT("A single Section edit dirties only its old and new Page owners"), Manager.DirtyFinePages.Num(), 2);
	TestFalse(TEXT("Fine mesh invalidation does not rebuild heightfield topology"), Manager.bHeightfieldTopologyDirty);
	FVoxelViewManager::FCoveragePassSnapshot Snapshot;
	const FVoxelViewKey Root{FIntVector::ZeroValue, 7};
	// A sparse line must never be mistaken for a complete volume.
	for (int32 X = 0; X < 128; ++X)
	{
		const FVoxelViewKey Slab{FIntVector(X, 0, 0), 0};
		Snapshot.FineNodes.Add(Slab);
		for (auto Node = Slab; Node.Level <= 8; Node = Node.GetParent()) Snapshot.FineBranches.Add(Node);
	}
	TestFalse(TEXT("A sparse line cannot falsely replace a 3D volume"), Manager.IsTerrainVolumeCovered(Root, Snapshot, true));
	Snapshot = {};
	const FVoxelViewKey Cube{FIntVector::ZeroValue, 3};
	for (int32 Z = 0; Z < 8; ++Z) for (int32 Y = 0; Y < 8; ++Y) for (int32 X = 0; X < 8; ++X)
	{
		const FVoxelViewKey Node{FIntVector(X, Y, Z), 0};
		Snapshot.FineNodes.Add(Node);
		for (auto Branch = Node; Branch.Level <= 8; Branch = Branch.GetParent()) Snapshot.FineBranches.Add(Branch);
	}
	TestTrue(TEXT("512 complete Fine Sections replace the Proxy volume"), Manager.IsTerrainVolumeCovered(Cube, Snapshot, true));
	Snapshot.FineNodes.Remove({FIntVector(7, 7, 7), 0});
	TestFalse(TEXT("One missing child keeps the previous Proxy"), Manager.IsTerrainVolumeCovered(Cube, Snapshot, true));
	Snapshot = {};
	const FVoxelViewKey LargeCube{FIntVector::ZeroValue, 6};
	for (uint8 Level = 0; Level <= LargeCube.Level; ++Level)
	{
		const int32 Side = 64 >> Level;
		for (int32 Z = 0; Z < Side; ++Z) for (int32 Y = 0; Y < Side; ++Y) for (int32 X = 0; X < Side; ++X)
		{
			const FVoxelViewKey Node{FIntVector(X, Y, Z), Level};
			Snapshot.FineBranches.Add(Node);
			if (Level == 0) Snapshot.FineNodes.Add(Node);
		}
	}
	TestTrue(TEXT("262144 Fine Sections with 65 edges on every axis completely replace a Proxy"),
		Manager.IsTerrainVolumeCovered(LargeCube, Snapshot, true));
	Snapshot.FineNodes.Remove({FIntVector(63, 63, 63), 0});
	TestFalse(TEXT("Large replacement still detects a single missing Fine Section"),
		Manager.IsTerrainVolumeCovered(LargeCube, Snapshot, true));
	TArray<FVoxelCoverageBox> Slabs;
	for (int32 X = 0; X < 128; ++X) Slabs.Add({FIntVector(X, 0, 0), FIntVector(X + 1, 128, 128)});
	TestTrue(TEXT("Generic coverage also accepts more than 64 edges"), VoxelCoverage::IsFullyCovered3D({FIntVector::ZeroValue, FIntVector(128)}, Slabs));
	Manager.Reset();
	const FVoxelFinePageKey OldLayout{FIntVector::ZeroValue, 2};
	FVoxelFinePagePlanner::GatherSections(OldLayout, Sections);
	for (const auto& Section : Sections)
	{
		const FVoxelFinePageKey Desired{Section, 0};
		Manager.GetInputRoot().FinePagePlan.Pages.Add(Desired);
		Manager.GetInputRoot().FinePagePlan.SectionOwners.Add(Section, Desired);
		Manager.GetInputRoot().FineWanted.Add(Section);
		Manager.FineReady.Add(Section);
		Manager.PresentedFineSectionOwners.Add(Section, OldLayout);
		const FVoxelViewKey Node{Section, 0};
		Manager.CommitTerrainOwnership({0, Section, 0}, MakeArrayView(&Node, 1));
	}
	Manager.ResolveTransitionVisibility();
	for (int32 Frame = 0; Frame < 8; ++Frame) Manager.Publisher->Tick();
	for (const auto& Section : Sections)
	{
		TestFalse(TEXT("F0 preparation and layout metadata cannot impersonate a Native Atom receipt"), Manager.IsFinePresented(Section));
		TestTrue(TEXT("Coverage does not rewrite an Atom owner through a whole-page publication"),
			Manager.PresentedFineSectionOwners.FindChecked(Section) == OldLayout);
	}
	for (const auto& Pair : Manager.VisibleTerrainNodesByGroup)
		TestTrue(TEXT("Fine ownership remains in per-Atom groups"), Pair.Key.Representation != 0 || Pair.Key.Level == 0);
	Manager.InvalidateFinePageLayout(OldLayout);
	Manager.RetireFinePageLayoutState(OldLayout);
	TestFalse(TEXT("Retiring obsolete layout metadata releases its dirty marker"), Manager.DirtyFinePages.Contains(OldLayout));
	TestEqual(TEXT("Layout retirement leaves Native Atom receipt ownership untouched"), Manager.PresentedFineSectionOwners.Num(), Sections.Num());
	Manager.Reset();
	TestTrue(TEXT("World reset clears the derived receipt ownership"), Manager.PresentedFineSectionOwners.IsEmpty());
	return true;
}

#endif
