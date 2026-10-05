#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Streaming/VoxelInterestManager.h"
#include "Voxel/Streaming/VoxelInterestRuntime.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Rendering/VoxelHeightfieldPageMesh.h"
#include "Voxel/Rendering/VoxelProxyPageMesh.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelKnownInitialInterestIntegrationTest,
	"WHFramework.Voxel.KnownRequirements.InitialInterestIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelKnownInitialInterestIntegrationTest::RunTest(const FString& InParameters)
{
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -256;
	Manifest.Settings.MaxZ = 256;
	FVoxelViewSettings Settings;
	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.Center = FIntVector(8);
	Source.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::FineVisual;
	Source.bInheritWorldView = false;
	Source.ExactRadius = -1;
	Source.View.FineRadiusCells = 1;
	Source.View.FineVerticalHalfExtentCells = 1;
	Source.CollisionRadius = 0;
	Source.SimulationRadius = 0;
	for (const int32 Initial : {16, 32})
	{
		Source.View.InitialChunkSideCells = Initial;
		const FVoxelInterestSet Interest = FVoxelInterestManager().Compute(MakeArrayView(&Source, 1), Manifest, Settings);
		TestTrue(TEXT("Real interest planner succeeds"), Interest.PlanningError.IsEmpty());
		if (!Interest.FinePagePlan)
		{
			AddError(TEXT("Real interest is missing the shared Fine page plan"));
			return false;
		}
		TestEqual(TEXT("Data halo never expands the visible initial family"), Interest.FinePagePlan->Pages.Num(), 1);
		TestEqual(TEXT("Real initial family contains all its Sections"), Interest.FinePagePlan->SectionOwners.Num(), Initial == 32 ? 8 : 1);
		for (const auto& Pair : Interest.FinePagePlan->SectionOwners)
		{
			const FVoxelExactDemand& Demand = Interest.Exact.FindChecked(Pair.Key);
			TestTrue(TEXT("Each merged Section retains real Fine data and render work"), Demand.bData && Demand.bFineData && Demand.bFineRender);
			TestFalse(TEXT("Page closure does not expand collision or simulation"), Demand.bCollision || Demand.bSimulation);
		}
	}
	FVoxelStreamingSource Previous = Source;
	Previous.View.InitialChunkSideCells = 16;
	TestTrue(TEXT("Size changes invalidate the effective source"), FVoxelInterestRuntime::NeedsUpdate(Previous, Source, Settings));
	Source.View.InitialChunkSideCells = 24;
	const FVoxelInterestSet Invalid = FVoxelInterestManager().Compute(MakeArrayView(&Source, 1), Manifest, Settings);
	TestFalse(TEXT("Real planner rejects invalid initial sizing"), Invalid.PlanningError.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelKnownEmptyPageIntegrationTest,
	"WHFramework.Voxel.KnownRequirements.EmptyPageIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelKnownEmptyPageIntegrationTest::RunTest(const FString& InParameters)
{
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler;
	FVoxelViewManager Manager(*Module, Scheduler, 1);
	const FVoxelFinePageKey Page{FIntVector::ZeroValue, 0};
	const FVoxelViewKey Node{FIntVector::ZeroValue, 0};
	Manager.FinePagePlan.Pages.Add(Page);
	Manager.FinePagePlan.SectionOwners.Add(Node.Coordinate, Page);
	Manager.FinePageReady.Add(Page);
	Manager.EmptyFinePages.Add(Page);
	Manager.PresentedEmptyFinePages.Add(Page);
	Manager.PresentedFineSectionOwners.Add(Node.Coordinate, Page);
	Manager.VisibleTerrainNodes.Add(Node);
	Manager.FineWanted.Add(Node.Coordinate);
	Manager.bCoverageDirty = false;
	Manager.bCoverageDirtyFull = false;
	const TSet<FVoxelViewKey> Target{Node};
	const FVoxelViewManager::FCoveragePassSnapshot Snapshot;
	for (int32 Iteration = 0; Iteration < 1000; ++Iteration)
	{
		const TOptional<bool> Visibility = Manager.UpdateFinePagePresentation(Page, Target, Snapshot);
		TestTrue(TEXT("Empty page retains visible ownership"), Visibility.IsSet() && Visibility.GetValue());
		TestFalse(TEXT("Unchanged empty commit is idempotent"), Manager.CommitFinePageOwnership(Page, MakeArrayView(&Node, 1), {}, Visibility));
	}
	TestTrue(TEXT("Stable empty pages create no publisher groups"), Manager.Publisher->PendingGroups.IsEmpty());
	TestFalse(TEXT("Stable empty pages do not dirty coverage"), Manager.bCoverageDirty);
	TestTrue(TEXT("Proven empty presentation is valid without an Actor"), Manager.IsFinePresented(Node.Coordinate));
	TestFalse(TEXT("A callback without a visibility change preserves committed ownership"), Manager.CommitFinePageOwnership(Page, {}, {Node.Coordinate}, {}));
	TestTrue(TEXT("A mesh-only callback preserves the committed empty presentation proof"), Manager.IsFinePresented(Node.Coordinate));
	Manager.DirtyFinePages.Add(Page);
	const TOptional<bool> DirtyVisibility = Manager.UpdateFinePagePresentation(Page, Target, Snapshot);
	TestTrue(TEXT("Pending mesh input does not change a stable empty page's visible ownership"), DirtyVisibility.IsSet() && DirtyVisibility.GetValue());
	TestTrue(TEXT("Mesh input dirtiness does not enqueue a no-op owner that blocks the replacement"), Manager.Publisher->PendingGroups.IsEmpty());
	Manager.FineReady.Add(Node.Coordinate);
	auto EmptyMesh = MakeShared<FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	TestTrue(TEXT("An empty result carries worker validation"), EmptyMesh->ValidateAndCache());
	Manager.FineSectionMeshes.Add(Node.Coordinate, EmptyMesh);
	TArray<FVoxelViewManager::FFinePageInput> Inputs;
	uint64 Signature = 0;
	TestTrue(TEXT("The current empty page has complete fine inputs"), Manager.CaptureFinePageInputs(Page, Inputs, Signature));
	Manager.PreparedFinePageMeshes.Add(Page, {Signature, EmptyMesh});
	Manager.bFinePresentationDirty = true;
	Manager.PrepareFinePages();
	TestFalse(TEXT("A prepared empty refresh consumes its dirty input"), Manager.DirtyFinePages.Contains(Page));
	TestTrue(TEXT("A prepared empty refresh creates no owner generation"), Manager.Publisher->PendingGroups.IsEmpty() && Manager.OpenPublishGroups.IsEmpty());
	TestTrue(TEXT("A prepared empty refresh retains committed presentation"), Manager.IsFinePresented(Node.Coordinate));
	Manager.EmptyFinePages.Remove(Page);
	Manager.PresentedEmptyFinePages.Remove(Page);
	TestFalse(TEXT("Lost Actor is not proof of empty terrain"), Manager.IsFinePresented(Node.Coordinate));
	TestFalse(TEXT("Lost nonempty Actor cannot publish as empty"), Manager.UpdateFinePagePresentation(Page, Target, Snapshot).IsSet());
	TestTrue(TEXT("Lost nonempty Actor schedules mesh recovery"), Manager.DirtyFinePages.Contains(Page));
	Manager.FinePageReady.Remove(Page);
	Manager.PresentedFineSectionOwners.Remove(Node.Coordinate);
	Manager.VisibleTerrainNodes.Remove(Node);
	TestFalse(TEXT("An unknown unprepared page does not report a committed presentation"), Manager.UpdateFinePagePresentation(Page, {}, Snapshot).IsSet());
	TestTrue(TEXT("An unprepared page cannot create an empty owner generation that blocks its own mesh preparation"), Manager.Publisher->PendingGroups.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelKnownSchedulerProgressIntegrationTest,
	"WHFramework.Voxel.KnownRequirements.SchedulerProgressIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelKnownSchedulerProgressIntegrationTest::RunTest(const FString& InParameters)
{
	for (const int32 WorkerCount : {1, 4})
	{
		FVoxelTaskScheduler Scheduler;
		FVoxelTaskBudget Budget;
		Budget.MaxConcurrentTasks = WorkerCount;
		Budget.MaxConcurrentCoarseTerrainTasks = WorkerCount;
		Budget.MaxConcurrentSurfaceTasks = WorkerCount;
		Budget.MaxConcurrentMacroTasks = WorkerCount;
		Scheduler.SetBudget(Budget);
		Scheduler.SetObservedTerrainStage(0);
		TAtomic<bool> Release(false);
		const EVoxelTaskKind Kinds[] = {EVoxelTaskKind::BuildCollision, EVoxelTaskKind::GenerateVoxelProxy,
			EVoxelTaskKind::GenerateSurface, EVoxelTaskKind::GenerateMacro, EVoxelTaskKind::BuildViewCoverage,
			EVoxelTaskKind::BuildSurface, EVoxelTaskKind::BuildMacro};
		int32 Completed[UE_ARRAY_COUNT(Kinds)] = {};
		uint64 NextCoverage = 100;
		auto Enqueue = [&](const EVoxelTaskKind Kind, const uint64 Token)
		{
			FVoxelTaskRequest Request;
			Request.Kind = Kind;
			Request.Stamp.Token = Token;
			Request.TerrainStage = Kind == EVoxelTaskKind::BuildViewCoverage ? 1 : INDEX_NONE;
			Request.WorkClass = Kind == EVoxelTaskKind::BuildCollision ? EVoxelWorkClass::Critical : EVoxelWorkClass::Visible;
			Request.ReservedBytes = 1024;
			Request.Execute = [&Release](const TAtomic<bool>& Cancel)
			{
				while (!Release.Load() && !Cancel.Load()) FPlatformProcess::Sleep(0.001f);
				FVoxelTaskResult Result;
				Result.bSuccess = !Cancel.Load();
				return Result;
			};
			return Scheduler.Enqueue(MoveTemp(Request));
		};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Kinds); ++Index)
		{
			TestTrue(TEXT("Real scheduler accepts each service lane"), Enqueue(Kinds[Index], Index + 1));
		}
		TestFalse(TEXT("Duplicate acceptance cannot leak producer reservations"), Enqueue(Kinds[1], 2));
		Release.Store(true);
		const double Deadline = FPlatformTime::Seconds() + 5.0;
		while (FPlatformTime::Seconds() < Deadline)
		{
			Enqueue(EVoxelTaskKind::BuildViewCoverage, ++NextCoverage);
			Scheduler.Tick([&](FVoxelTaskResult&& Result)
			{
				if (Result.Stamp.Token <= UE_ARRAY_COUNT(Kinds) && Result.bSuccess && !Result.bCanceled)
				{
					++Completed[Result.Stamp.Token - 1];
				}
			}, 8.0);
			bool bFinished = true;
			for (const int32 Count : Completed) bFinished &= Count == 1;
			if (bFinished) break;
			FPlatformProcess::Sleep(0.001f);
		}
		for (const int32 Count : Completed) TestEqual(TEXT("Each real lane progresses under continuous Coverage"), Count, 1);
		Scheduler.SetObservedTerrainStage(0);
		Scheduler.StopAndJoin();
	}
	return true;
}


namespace
{
	double TerrainMeshArea(const FVoxelSectionMeshResult& Mesh)
	{
		double Area = 0.0;
		for (const auto& Batch : Mesh.Batches)
			for (int32 Index = 0; Index < Batch.Mesh.Triangles.Num(); Index += 3)
			{
				const FVector A = Batch.Mesh.Vertices[Batch.Mesh.Triangles[Index]];
				const FVector B = Batch.Mesh.Vertices[Batch.Mesh.Triangles[Index + 1]];
				const FVector C = Batch.Mesh.Vertices[Batch.Mesh.Triangles[Index + 2]];
				Area += FVector::CrossProduct(B - A, C - A).Length() * 0.5;
			}
		return Area;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelResumablePageIntegrationTest,
	"WHFramework.Voxel.KnownRequirements.ResumablePageIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelResumablePageIntegrationTest::RunTest(const FString& InParameters)
{
	FVoxelTaskScheduler Scheduler;
	FVoxelTaskBudget Budget;
	Budget.MaxConcurrentTasks = 1;
	Scheduler.SetBudget(Budget);
	TAtomic<int32> Executions(0);
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::BuildVoxelProxy;
	Request.WorkClass = EVoxelWorkClass::Visible;
	Request.Stamp.Token = 701;
	Request.InputBytes = 1024;
	Request.ReservedBytes = 1024;
	Request.bResumable = true;
	Request.Execute = [&Executions](const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Result;
		Result.bSuccess = !Cancel.Load();
		Result.bContinue = ++Executions < 8;
		return Result;
	};
	TestTrue(TEXT("Logical page admitted"), Scheduler.Enqueue(MoveTemp(Request)));
	int32 Applied = 0;
	const double Deadline = FPlatformTime::Seconds() + 5.0;
	while (Scheduler.ActiveCount() && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([&](FVoxelTaskResult&& Result)
		{
			TestFalse(TEXT("Partial shard is never applied"), Result.bContinue);
			++Applied;
		});
		FPlatformProcess::Sleep(0.001f);
	}
	TestEqual(TEXT("All eight original-queue windows execute"), Executions.Load(), 8);
	TestEqual(TEXT("The completed logical page applies exactly once"), Applied, 1);
	TestEqual(TEXT("Completion releases logical ownership"), Scheduler.ActiveCount(), 0);
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHeightfieldWindowIntegrationTest,
	"WHFramework.Voxel.KnownRequirements.HeightfieldWindowIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHeightfieldWindowIntegrationTest::RunTest(const FString& InParameters)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	FVoxelRegistrySnapshot RegistryValue = *VoxelTest::MakeRegistry();
	RegistryValue.Definitions[Config->Water.TypeId].RenderGroup = EVoxelRenderGroup::Water;
	const auto Registry = MakeShared<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe>(MoveTemp(RegistryValue));
	FVoxelSurfaceTileData Data;
	Data.Key = {FIntPoint(-1, 2), 2};
	Data.Geometry = {{EVoxelTerrainRepresentation::Surface, FIntVector(-1, 2, 0), 2}, 1, -64, 128, 17, 23};
	Data.Side = 65;
	Data.Step = 1;
	const int32 Count = Data.Side * Data.Side;
	Data.GroundZ.SetNum(Count);
	Data.SurfaceMaterial.Init(Config->Grass.TypeId, Count);
	Data.WaterZ.Init(MIN_int32, Count);
	Data.Flags.Init(0, Count);
	for (int32 Y = 0; Y < Data.Side; ++Y)
		for (int32 X = 0; X < Data.Side; ++X)
		{
			const int32 Index = X + Y * Data.Side;
			Data.GroundZ[Index] = X < 32 ? 7 : 12;
			if (Y < 20) { Data.WaterZ[Index] = 15; Data.Flags[Index] = VoxelSurface_Lake; }
		}
	FString Error;
	FVoxelWaterSurfaceTileData Water;
	if (!FVoxelWaterViewBuilder().Build(Data, Water, Error)) { AddError(Error); return false; }
	FVoxelSectionMeshResult Whole, WaterMesh, Sliced;
	if (!FVoxelHeightfieldMesher::BuildBlockyTerrain(Data.Side, Data.Step, Data.GroundZ,
		Data.SurfaceMaterial, *Config, *Registry, Whole, Error) ||
		!FVoxelHeightfieldMesher::BuildWater(Water, *Registry, WaterMesh, Error)) { AddError(Error); return false; }
	Whole.Batches.Append(MoveTemp(WaterMesh.Batches));
	FVoxelHeightfieldPageMesh Page;
	if (!Page.Initialize(Data.Geometry, Error)) { AddError(Error); return false; }
	bool Complete = false;
	int32 Windows = 0;
	while (!Complete && Windows < 20)
	{
		if (!Page.Advance(Data, Water, *Config, *Registry, 32.0, Sliced, Complete, Error, nullptr))
		{ AddError(Error); return false; }
		++Windows;
		if (!Complete) TestTrue(TEXT("Intermediate window never exposes a parent mesh"), Sliced.Batches.IsEmpty());
	}
	TestTrue(TEXT("All windows complete"), Complete);
	TestTrue(TEXT("Several independent windows execute"), Windows > 4);
	TestTrue(TEXT("Cross-window height steps and shoreline preserve exact surface area"),
		FMath::IsNearlyEqual(TerrainMeshArea(Whole), TerrainMeshArea(Sliced), 0.001));
	TestTrue(TEXT("Completed mesh carries worker validation"), Sliced.bValidated);
	FVoxelHeightfieldPageMesh Counter;
	if (!Counter.Initialize(Data.Geometry, Error, 0, true)) { AddError(Error); return false; }
	FVoxelSectionMeshResult Counted;
	bool CountComplete = false;
	for (int32 Index = 0; Index < 20 && !CountComplete; ++Index)
		if (!Counter.Advance(Data, Water, *Config, *Registry, 32.0, Counted, CountComplete, Error, nullptr))
		{ AddError(Error); return false; }
	TestTrue(TEXT("Shoreline preflight completes without vertex allocations"), CountComplete && Counted.Batches.IsEmpty());
	TestEqual(TEXT("Preflight includes terrain, water and clipped shoreline polygons"), Counter.GetEstimatedMeshBytes(), Sliced.Bytes());
	FVoxelSurfaceTileData Changed = Data;
	Changed.Geometry.SampleStepCells = 2;
	bool Ignored = false;
	TestFalse(TEXT("Same spatial key cannot reuse a different sample geometry"),
		Page.Advance(Changed, Water, *Config, *Registry, 32.0, Sliced, Ignored, Error, nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHeightfieldCapacityIntegrationTest,
	"WHFramework.Voxel.KnownRequirements.HeightfieldCapacityIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHeightfieldCapacityIntegrationTest::RunTest(const FString& InParameters)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	FVoxelRegistrySnapshot RegistryValue = *VoxelTest::MakeRegistry();
	RegistryValue.Definitions[Config->Water.TypeId].RenderGroup = EVoxelRenderGroup::Water;
	const auto Registry = MakeShared<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe>(MoveTemp(RegistryValue));
	FVoxelSurfaceTileData Data;
	Data.Key = {FIntPoint::ZeroValue, 4};
	Data.Geometry = {{EVoxelTerrainRepresentation::Surface, FIntVector::ZeroValue, 4}, 1, -64, 128, 17, 23};
	Data.Side = 257;
	Data.Step = 1;
	const int32 Count = Data.Side * Data.Side;
	Data.GroundZ.Init(8, Count);
	Data.SurfaceMaterial.Init(Config->Grass.TypeId, Count);
	Data.WaterZ.Init(MIN_int32, Count);
	Data.Flags.Init(0, Count);
	FString Error;
	FVoxelWaterSurfaceTileData Water;
	if (!FVoxelWaterViewBuilder().Build(Data, Water, Error)) { AddError(Error); return false; }
	FVoxelHeightfieldPageMesh Counter;
	if (!Counter.Initialize(Data.Geometry, Error, 0, true)) { AddError(Error); return false; }
	FVoxelSectionMeshResult Mesh;
	bool Complete = false;
	for (int32 Index = 0; Index < 100 && !Complete; ++Index)
		if (!Counter.Advance(Data, Water, *Config, *Registry, 32.0, Mesh, Complete, Error, nullptr))
		{ AddError(Error); return false; }
	TestTrue(TEXT("Large legal page preflight completes"), Complete);
	const uint64 Capacity = Counter.GetEstimatedMeshBytes();
	TestTrue(TEXT("Fixture exceeds the former fixed mesh capacity"), Capacity > 12ull * 1024ull * 1024ull);
	FVoxelHeightfieldPageMesh Page;
	if (!Page.Initialize(Data.Geometry, Error, Capacity)) { AddError(Error); return false; }
	Complete = false;
	for (int32 Index = 0; Index < 100 && !Complete; ++Index)
		if (!Page.Advance(Data, Water, *Config, *Registry, 32.0, Mesh, Complete, Error, nullptr))
		{ AddError(Error); return false; }
	TestTrue(TEXT("Large parent publishes only after all execution windows complete"), Complete && Mesh.bValidated);
	TestEqual(TEXT("Exact preflight bounds the real retained mesh"), Mesh.Bytes(), Capacity);
	TestTrue(TEXT("Complete parent retains its full spatial surface"),
		FMath::IsNearlyEqual(TerrainMeshArea(Mesh), 256.0 * 256.0, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelProxyWindowIntegrationTest,
	"WHFramework.Voxel.KnownRequirements.ProxyWindowIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelProxyWindowIntegrationTest::RunTest(const FString& InParameters)
{
	const auto Registry = VoxelTest::MakeRegistry();
	FVoxelShapeRegistry Shapes;
	Shapes.BuildDefaults();
	FVoxelVoxelProxyData Data;
	Data.Key = {FIntVector::ZeroValue, 1};
	Data.Geometry = {{EVoxelTerrainRepresentation::VoxelProxy, FIntVector::ZeroValue, 1}, 1, -64, 128, 17, 23};
	Data.GridSide = 32;
	Data.Cells.Init(FVoxelBlockState{}, 32 * 32 * 32);
	for (int32 Z = 0; Z < 19; ++Z)
		for (int32 Y = 0; Y < 32; ++Y)
			for (int32 X = 0; X < 32; ++X) Data.Cells[X + 32 * (Y + 32 * Z)].TypeId = 1;
	FVoxelSectionSnapshot Snapshot;
	Snapshot.GridSide = 32;
	for (const auto& Cell : Data.Cells) Snapshot.Blocks.Add(Cell.Pack());
	for (int32 Face = 0; Face < 6; ++Face)
	{
		Data.Known[Face] = Snapshot.Known[Face] = true;
		Data.Halo[Face].Init(FVoxelBlockState{}, 32 * 32);
		Snapshot.Halo[Face].Init(0, 32 * 32);
	}
	FVoxelSectionMeshResult Whole, Sliced;
	TestTrue(TEXT("Reference full parent builds"), FVoxelSectionMesher::Build(Snapshot, *Registry, Shapes, Whole));
	FVoxelProxyPageMesh Page;
	FString Error;
	if (!Page.Initialize(Data.Geometry, Error)) { AddError(Error); return false; }
	bool Complete = false;
	for (int32 Window = 0; Window < 12 && !Complete; ++Window)
		if (!Page.Advance(Data, *Registry, Shapes, nullptr, 1.0, Sliced, Complete, Error, nullptr))
		{ AddError(Error); return false; }
	TestTrue(TEXT("Parent mesh finishes after bounded windows"), Complete);
	TestTrue(TEXT("Reference includes actual block surfaces"), TerrainMeshArea(Whole) > 0.0);
	TestTrue(TEXT("Internal shard halo prevents extra faces or holes"),
		FMath::IsNearlyEqual(TerrainMeshArea(Whole), TerrainMeshArea(Sliced), 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelStageArrayIntegrationTest,
	"WHFramework.Voxel.KnownRequirements.StageArrayIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStageArrayIntegrationTest::RunTest(const FString& InParameters)
{
	(void)InParameters;

	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -64;
	Manifest.Settings.MaxZ = 128;

	FVoxelViewSettings Settings;
	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.Center = FIntVector(8);
	Source.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::FineVisual | EVoxelStreamingCapability::WorldVisual;
	Source.bInheritWorldView = false;
	Source.ExactRadius = -1;
	Source.View.FineRadiusCells = 16;
	Source.View.FineVerticalHalfExtentCells = 16;
	Source.View.VoxelProxyRadiusCells = 64;
	Source.View.VoxelProxyVerticalHalfExtentCells = 64;
	Source.View.SurfaceRadiusCells = 128;
	Source.View.MacroRadiusCells = 256;

	const auto Interest = FVoxelInterestManager().Compute(MakeArrayView(&Source, 1), Manifest, Settings);
	if (!Interest.PlanningError.IsEmpty())
	{
		AddError(Interest.PlanningError);
		return false;
	}

	const auto* Accepted = Interest.Sources.Find(Source.Id);
	TestNotNull(TEXT("Effective source is retained"), Accepted);
	if (!Accepted)
	{
		return false;
	}

	const auto MinimumSide = [&Interest](const EVoxelTerrainRepresentation Representation)
	{
		int32 Result = MAX_int32;
		for (const auto& Pair : Interest.Geometry)
		{
			if (Pair.Key.Representation == Representation)
			{
				Result = FMath::Min(Result, static_cast<int32>(Pair.Key.SideCells()));
			}
		}
		return Result == MAX_int32 ? 16 : Result;
	};

	TestEqual(TEXT("Proxy first side reports the actual plan"), Accepted->FirstSideCells[0], MinimumSide(EVoxelTerrainRepresentation::VoxelProxy));
	TestEqual(TEXT("Surface first side reports the actual plan"), Accepted->FirstSideCells[1], MinimumSide(EVoxelTerrainRepresentation::Surface));
	TestEqual(TEXT("Macro first side reports the actual plan"), Accepted->FirstSideCells[2], MinimumSide(EVoxelTerrainRepresentation::Macro));

	for (const int32 Side : Accepted->FirstSideCells)
	{
		TestTrue(TEXT("Recorded page side is a valid power-of-two side"), Side >= 16 && FMath::IsPowerOfTwo(static_cast<uint32>(Side)));
	}

	return true;
}

#endif
