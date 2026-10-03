#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationPlanCoordinator.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "Voxel/Tests/VoxelTestUtilities.h"
#include "Async/Async.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelEnvironmentCacheReadTest,
	"WHFramework.Voxel.Generation.EnvironmentCacheRead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEnvironmentCacheReadTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>(false, false);
	const FVoxelGenerationPipeline Pipeline(VoxelTest::MakeGenerationConfig(), Cache);
	const FIntPoint Cell(-17, 31);
	FVoxelNaturalColumnEntryPtr ColdEntry;
	FString ColdError;
	int32 GameThreadBuilds = 0;
	TestFalse(TEXT("Cold GetOrBuild cannot invoke a runtime GameThread builder"), Cache->GetOrBuildNaturalColumn(Cell,
		[&GameThreadBuilds](FVoxelNaturalColumnEntry&, FString&) { ++GameThreadBuilds; return true; }, ColdEntry, ColdError));
	TestEqual(TEXT("Runtime builder is never entered on GameThread"), GameThreadBuilds, 0);
	TestTrue(TEXT("Cold GetOrBuild reports a retryable dependency"), ColdError.StartsWith(TEXT("DependencyNotReady")));
	FVoxelEnvironmentSample Sample;
	TestFalse(TEXT("Cold environment read does not build on GameThread"), Pipeline.TrySampleEnvironment(Cell, Sample));
	TestEqual(TEXT("Cold read leaves natural cache empty"), Cache->GetStats().NaturalColumns, 0);
	TAtomic<bool> BuildEntered(false);
	TAtomic<bool> ReleaseBuild(false);
	TFuture<bool> Build = Async(EAsyncExecution::ThreadPool, [Cache, Cell, &BuildEntered, &ReleaseBuild]()
	{
		FVoxelNaturalColumnEntryPtr Entry;
		FString Error;
		return Cache->GetOrBuildNaturalColumn(Cell,
			[Cell, &BuildEntered, &ReleaseBuild](FVoxelNaturalColumnEntry& OutEntry, FString&)
			{
				BuildEntered.Store(true);
				while (!ReleaseBuild.Load()) FPlatformProcess::Sleep(0.001f);
				OutEntry.Position = Cell;
				OutEntry.Column.SurfaceZ = 237;
				OutEntry.Column.Climate.MoistureQ15 = -1234;
				return true;
			}, Entry, Error);
	});
	const double Deadline = FPlatformTime::Seconds() + 5.0;
	while (!BuildEntered.Load() && FPlatformTime::Seconds() < Deadline) FPlatformProcess::Sleep(0.001f);
	TestFalse(TEXT("A read during a worker build returns pending immediately"), Pipeline.TrySampleEnvironment(Cell, Sample));
	ReleaseBuild.Store(true);
	TestTrue(TEXT("Worker publishes natural column"), Build.Get());
	TestTrue(TEXT("Prepared environment reads on GameThread"), Pipeline.TrySampleEnvironment(Cell, Sample));
	TestTrue(TEXT("Warm GetOrBuild remains readable on GameThread"), Cache->GetOrBuildNaturalColumn(Cell,
		[&GameThreadBuilds](FVoxelNaturalColumnEntry&, FString&) { ++GameThreadBuilds; return true; }, ColdEntry, ColdError));
	TestEqual(TEXT("Warm read does not enter its builder"), GameThreadBuilds, 0);
	TestEqual(TEXT("Read preserves surface height"), Sample.Column.SurfaceZ, 237);
	TestEqual(TEXT("Read preserves climate"), Sample.Column.Climate.MoistureQ15, -1234);
	TestEqual(TEXT("Read never waits on a build gate"), Cache->GetStats().GateWaitCount, 0ull);
	Cache->Reset();
	TestFalse(TEXT("World cache reset invalidates environment"), Pipeline.TrySampleEnvironment(Cell, Sample));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelGenerationCacheNoWaitTest,
	"WHFramework.Voxel.Generation.CacheNoWait",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelGenerationCacheNoWaitTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationPlanCache Cache(false, false);
	FVoxelHydrologyRegionKey Key;
	Key.Coordinate = FIntPoint(2, -3);
	TAtomic<bool> BuildEntered(false);
	TAtomic<bool> ReleaseBuild(false);
	TFuture<bool> Owner = Async(EAsyncExecution::ThreadPool,
		[&Cache, &Key, &BuildEntered, &ReleaseBuild]()
		{
			FVoxelHydrologyPlanPtr Plan;
			FString Error;
			return Cache.GetOrBuildHydrology(Key,
				[&BuildEntered, &ReleaseBuild](FVoxelHydrologyPlan&, FString&)
				{
					BuildEntered.Store(true);
					while (!ReleaseBuild.Load()) FPlatformProcess::Sleep(0.001f);
					return true;
				}, Plan, Error);
		});
	const double Deadline = FPlatformTime::Seconds() + 5.0;
	while (!BuildEntered.Load() && FPlatformTime::Seconds() < Deadline)
	{
		FPlatformProcess::Sleep(0.001f);
	}
	FVoxelHydrologyPlanPtr PendingPlan;
	FString PendingError;
	const bool bWaited = Cache.GetOrBuildHydrology(Key,
		[](FVoxelHydrologyPlan&, FString&) { return true; },
		PendingPlan, PendingError);
	ReleaseBuild.Store(true);
	const bool bOwnerSucceeded = Owner.Get();
	TestTrue(TEXT("Owner begins building"), BuildEntered.Load());
	TestFalse(TEXT("Concurrent consumer returns without waiting"), bWaited);
	TestTrue(TEXT("Concurrent consumer reports pending dependency"),
		PendingError.StartsWith(TEXT("DependencyNotReady")));
	TestTrue(TEXT("Owner completes"), bOwnerSucceeded);
	TestTrue(TEXT("Completed plan is cached"), Cache.FindHydrology(Key, PendingPlan));
	TestEqual(TEXT("No cache gate wait is recorded"), Cache.GetStats().GateWaitCount, 0ull);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelGenerationCacheTaskRetentionTest,
	"WHFramework.Voxel.Generation.CacheTaskRetention",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelGenerationCacheTaskRetentionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelGenerationCacheRetention Streaming;
	Streaming.HydrologyRegionSide = 8;
	Streaming.HydrologyCellSize = 1;
	Streaming.Revision = 1;
	FVoxelGenerationCacheRetentionPoint& StreamingPoint = Streaming.Points.AddDefaulted_GetRef();
	StreamingPoint.Center = FIntPoint(4, 4);
	StreamingPoint.HydrologyRadiusCells = 1;
	Cache->UpdateRetention(Streaming);

	FVoxelHydrologyRegionKey StreamingKey;
	StreamingKey.Coordinate = FIntPoint::ZeroValue;
	FVoxelHydrologyRegionKey TaskKey;
	TaskKey.Coordinate = FIntPoint(5, 0);
	Cache->StoreHydrology(StreamingKey,
		MakeShared<FVoxelHydrologyPlan, ESPMode::ThreadSafe>());
	Cache->StoreHydrology(TaskKey,
		MakeShared<FVoxelHydrologyPlan, ESPMode::ThreadSafe>());
	FVoxelGenerationCacheRetentionPoint TaskPoint;
	TaskPoint.Center = FIntPoint(44, 4);
	TaskPoint.HydrologyRadiusCells = 1;
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Lease =
		Cache->RetainForTask(TaskPoint);
	for (int32 Index = 0; Index < FVoxelGenerationPlanCache::ShardCount * 2; ++Index)
	{
		Cache->TickMaintenance(256);
	}
	FVoxelHydrologyPlanPtr Plan;
	TestTrue(TEXT("Task plan survives streaming maintenance"),
		Cache->FindHydrology(TaskKey, Plan));
	TestTrue(TEXT("Streaming plan remains retained"),
		Cache->FindHydrology(StreamingKey, Plan));
	Lease.Reset();
	for (int32 Index = 0; Index < FVoxelGenerationPlanCache::ShardCount * 2; ++Index)
	{
		Cache->TickMaintenance(256);
	}
	TestFalse(TEXT("Task plan can be evicted after task completion"),
		Cache->FindHydrology(TaskKey, Plan));
	TestTrue(TEXT("Streaming plan remains retained after task completion"),
		Cache->FindHydrology(StreamingKey, Plan));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelGenerationPlanCoordinatorTest,
	"WHFramework.Voxel.Generation.PlanCoordinator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelGenerationPlanCoordinatorTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelTaskScheduler Scheduler;
	FVoxelTaskBudget Budget;
	Budget.MaxConcurrentTasks = 1;
	Scheduler.SetBudget(Budget);
	FVoxelGenerationPlanCoordinator Coordinator(Scheduler, Config, Cache, 7);
	FVoxelGenerationPlanDependency Dependency;
	Dependency.Kind = EVoxelGenerationPlanKind::Hydrology;
	Dependency.HydrologyKey.Coordinate = FIntPoint(0, 0);
	Dependency.StableHash = 713;
	const TArray<FVoxelGenerationPlanDependency> Dependencies { Dependency };
	const FVoxelGenerationDependencyStatus First = Coordinator.Ensure(
		Dependencies, EVoxelWorkClass::Visible, 3, 128.0, 0.0);
	TestFalse(TEXT("First consumer waits outside worker"), First.bReady);
	TestEqual(TEXT("One task builds the shared plan"), Scheduler.ActiveCount(), 1);
	Coordinator.Ensure(Dependencies, EVoxelWorkClass::Critical, 0, 8.0, 1.0);
	TestEqual(TEXT("Second consumer reuses queued task"), Scheduler.ActiveCount(), 1);
	const FVoxelTaskDiagnostics Priority = Scheduler.GetDiagnostics();
	TestEqual(TEXT("Priority donation promotes queued or running plan"),
		Priority.CriticalPending + Priority.CriticalRunning, 1);
	FVoxelGenerationDependencyStatus Status;
	const double Deadline = FPlatformTime::Seconds() + 30.0;
	do
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		Status = Coordinator.Ensure(Dependencies, EVoxelWorkClass::Critical, 0, 8.0, 1.0);
		if (!Status.bReady) FPlatformProcess::Sleep(0.001f);
	} while (!Status.bReady && !Status.bFailed && FPlatformTime::Seconds() < Deadline);
	Scheduler.StopAndJoin();
	TestFalse(TEXT("Shared plan succeeds"), Status.bFailed);
	TestTrue(TEXT("Shared plan becomes ready"), Status.bReady);
	TestEqual(TEXT("Single plan build does not wait on cache gate"),
		Cache->GetStats().GateWaitCount, 0ull);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelGenerationPlanPreflightTest,
	"WHFramework.Voxel.Generation.PlanPreflight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelGenerationPlanPreflightTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>(false);
	FVoxelTaskScheduler Scheduler;
	FVoxelTaskBudget Budget;
	Budget.MaxConcurrentTasks = 4;
	Budget.CriticalReservedTasks = 1;
	Scheduler.SetBudget(Budget);
	FVoxelGenerationPlanCoordinator Coordinator(Scheduler, Config, Cache, 11);
	TArray<FVoxelGenerationPlanDependency> Dependencies;
	FString Error;
	if (!TestTrue(TEXT("Section dependencies are gathered"),
		Coordinator.GatherForBounds({ FIntVector::ZeroValue, FIntVector(16) },
			Dependencies, Error))) return false;
	TestTrue(TEXT("Section has shared plan dependencies"), !Dependencies.IsEmpty());
	FVoxelGenerationCacheRetention StreamingRetention;
	StreamingRetention.HydrologyRegionSide = Config->Recipe->Settings.HydrologyRegionSide;
	StreamingRetention.HydrologyCellSize = Config->Recipe->Settings.HydrologyCellSize;
	StreamingRetention.Revision = 1;
	FVoxelGenerationCacheRetentionPoint& StreamingPoint = StreamingRetention.Points.AddDefaulted_GetRef();
	StreamingPoint.NaturalRadiusCells = StreamingPoint.PlanRadiusCells = StreamingPoint.HydrologyRadiusCells = 32;
	Cache->UpdateRetention(StreamingRetention);
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> ConsumerRetention =
		Coordinator.RetainDependencies(Dependencies, FIntPoint(8, 8));
	FVoxelGenerationDependencyStatus Status;
	const double Deadline = FPlatformTime::Seconds() + 90.0;
	do
	{
		Status = Coordinator.Ensure(Dependencies, EVoxelWorkClass::Critical, 0, 0.0, 1.0);
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		Cache->TickMaintenance(256);
		if (!Status.bReady && !Status.bFailed) FPlatformProcess::Sleep(0.001f);
	} while (!Status.bReady && !Status.bFailed && FPlatformTime::Seconds() < Deadline);
	Scheduler.StopAndJoin();
	if (!TestFalse(TEXT("Section preflight does not fail: ") + Status.Error,
		Status.bFailed) ||
		!TestTrue(TEXT("Section preflight completes"), Status.bReady)) return false;
	const FVoxelGenerationPipeline PreflightGenerator(Config, Cache, nullptr, true);
	TArray<FVoxelBlockState> PreflightBlocks;
	TArray<FVoxelBlockState> ReferenceBlocks;
	FString ReferenceError;
	const auto ReferenceGenerator = VoxelTest::MakeGenerator();
	TFuture<bool> Generated = Async(EAsyncExecution::ThreadPool,
		[&PreflightGenerator, &PreflightBlocks, &ReferenceBlocks,
			&Error, &ReferenceError, ReferenceGenerator]()
		{
			return PreflightGenerator.GenerateSection(FIntVector::ZeroValue,
				PreflightBlocks, Error) &&
				ReferenceGenerator->GenerateSection(FIntVector::ZeroValue,
					ReferenceBlocks, ReferenceError);
		});
	if (!TestTrue(TEXT("Worker exact generation consumes ready plans"),
		Generated.Get()))
	{
		AddError(Error);
		if (!ReferenceError.IsEmpty()) AddError(ReferenceError);
		return false;
	}
	TestEqual(TEXT("Preflight preserves exact generation"),
		VoxelTest::HashBlocks(PreflightBlocks), VoxelTest::HashBlocks(ReferenceBlocks));
	TestEqual(TEXT("Preflight avoids cache gate waits"), Cache->GetStats().GateWaitCount, 0ull);
	TestTrue(TEXT("A narrow streaming source keeps the consumer's completed hydrology available"), Cache->GetStats().Hydrology > 0);
	ConsumerRetention.Reset();
	for (int32 Index = 0; Index < FVoxelGenerationPlanCache::ShardCount * 4; ++Index) Cache->TickMaintenance(256);
	TestEqual(TEXT("Consumer completion releases plans outside the narrow streaming retention"), Cache->GetStats().Hydrology, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelGenerationPlanKeysTest,
	"WHFramework.Voxel.Generation.PlanKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCoarseEnvironmentPreflightTest,
	"WHFramework.Voxel.Generation.CoarseEnvironmentPreflight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCoarseEnvironmentPreflightTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>(false, false);
	const FVoxelGenerationPipeline Pipeline(Config, Cache, nullptr, true);
	FString Error;
	TArray<FVoxelColumnSample> Columns;
	const bool bColdSuccess = Async(EAsyncExecution::ThreadPool, [&]()
	{
		return Pipeline.SampleColumns(FIntPoint(-1, -1), 3, 3, 257, Columns, Error);
	}).Get();
	TestFalse(TEXT("Cold runtime coarse data cannot construct its own hydrology"), bColdSuccess);
	TestTrue(TEXT("Cold coarse data yields to preflight"), Error.StartsWith(TEXT("DependencyNotReady")));
	TestEqual(TEXT("Cold coarse worker leaves plan cache unbuilt"), Cache->GetStats().Hydrology, 0);
	FVoxelTaskScheduler Scheduler;
	FVoxelGenerationPlanCoordinator Coordinator(Scheduler, Config, Cache, 31);
	const FVoxelGenerationBounds Bounds{FIntVector(-1, -1, -4096), FIntVector(514, 514, 4096)};
	FVoxelGenerationDependencyStatus Status;
	const double Deadline = FPlatformTime::Seconds() + 30.0;
	do
	{
		Status = Coordinator.EnsureEnvironmentBounds(Bounds, EVoxelWorkClass::Prefetch, 0, 128.0, 0.0);
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		if (!Status.bReady && !Status.bFailed) FPlatformProcess::Sleep(0.001f);
	} while (!Status.bReady && !Status.bFailed && FPlatformTime::Seconds() < Deadline);
	Scheduler.StopAndJoin();
	if (!TestTrue(TEXT("Cross-boundary environment plans complete"), Status.bReady))
	{
		if (!Status.Error.IsEmpty()) AddError(Status.Error);
		return false;
	}
	const auto Before = Cache->GetStats();
	TestEqual(TEXT("Negative and positive grid bounds cover nine hydrology regions"), Before.Hydrology, 9);
	TestEqual(TEXT("Coarse preflight does not request volumetric cave plans"), Before.Caves, 0);
	TestEqual(TEXT("Coarse preflight does not request ecology tile plans"), Before.Ecology, 0);
	TestEqual(TEXT("Coarse preflight does not request feature plans"), Before.Features, 0);
	TestEqual(TEXT("Coarse preflight does not request structure plans"), Before.Structures, 0);
	TestTrue(TEXT("Runtime coarse worker consumes ready environment plans"),
		Async(EAsyncExecution::ThreadPool, [&]()
		{
			return Pipeline.SampleColumns(FIntPoint(-1, -1), 3, 3, 257, Columns, Error);
		}).Get());
	TestEqual(TEXT("All coarse samples are produced"), Columns.Num(), 9);
	TestEqual(TEXT("Coarse consumption builds no additional hydrology"), Cache->GetStats().Hydrology, Before.Hydrology);
	TestEqual(TEXT("Coarse generation never waits on a cache gate"), Cache->GetStats().GateWaitCount, 0ull);
	return true;
}

bool FVoxelGenerationPlanKeysTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationSettings Settings;
	Settings.CaveSpacing = 256;
	Settings.Ecology.Tree.CrownRadius = 0;
	Settings.Ecology.Grass.PatchRadius = 0;
	const FVoxelGenerationBounds Bounds {
		FIntVector(-16, -16, 0), FIntVector(0, 0, 16)
	};
	const FVoxelGenerationPlanKeys Keys = FVoxelGenerationQuery::GatherPlanKeys(
		Bounds, Settings);
	TestEqual(TEXT("Negative boundary uses nine plan tiles"), Keys.Tiles.Num(), 9);
	TestTrue(TEXT("Negative owner tile included"), Keys.Tiles.Contains(
		FVoxelGenerationTileKey { FIntVector(-1, -1, 0) }));
	TestTrue(TEXT("Positive neighbor tile included"), Keys.Tiles.Contains(
		FVoxelGenerationTileKey { FIntVector(0, 0, 0) }));
	TestEqual(TEXT("Ecology margin crosses four tiles"), Keys.EcologyTiles.Num(), 4);
	TestTrue(TEXT("Negative ecology tile included"), Keys.EcologyTiles.Contains(
		FVoxelEcologyTileKey { FIntPoint(-1, -1) }));
	TArray<FVoxelGenerationTileKey> StructureTiles;
	FVoxelGenerationQuery::GatherStructureTiles(Bounds, StructureTiles);
	TestEqual(TEXT("Structure preflight matches plan margin"), StructureTiles.Num(), 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelGenerationMathDeterminismTest,
	"WHFramework.Voxel.Generation.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelGenerationMathDeterminismTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	static const FIntVector Coordinates[] =
	{
		FIntVector::ZeroValue,
		FIntVector(1, -1, 0),
		FIntVector(-16, 32, 4),
		FIntVector(128, -64, -3)
	};
	for (const FIntVector& Coordinate : Coordinates)
	{
		const uint64 SeedA = VoxelGeneration::MakeSeed(173, Coordinate, 0x9e3779b97f4a7c15ull);
		const uint64 SeedB = VoxelGeneration::MakeSeed(173, Coordinate, 0x9e3779b97f4a7c15ull);
		TestEqual(TEXT("Fixed seed and coordinate are stable"), SeedA, SeedB);
		TestEqual(
			TEXT("3D noise is stable"),
			VoxelGeneration::Noise3D(173, Coordinate, 64, 31),
			VoxelGeneration::Noise3D(173, Coordinate, 64, 31));
	}
	TestEqual(TEXT("Negative floor division"), VoxelGeneration::FloorDivide(-1, 16), -1);
	TestEqual(TEXT("Negative exact floor division"), VoxelGeneration::FloorDivide(-16, 16), -1);
	TestTrue(
		TEXT("Different seed changes stable identity"),
		VoxelGeneration::MakeStableId(173, Coordinates[2], 7) !=
		VoxelGeneration::MakeStableId(174, Coordinates[2], 7));

	const TSharedRef<FVoxelGenerationPipeline, ESPMode::ThreadSafe> GeneratorA = VoxelTest::MakeGenerator();
	const TSharedRef<FVoxelGenerationPipeline, ESPMode::ThreadSafe> GeneratorB = VoxelTest::MakeGenerator();
	for (const FIntVector& Coordinate : Coordinates)
	{
		TArray<FVoxelBlockState> BaseA;
		TArray<FVoxelBlockState> BaseB;
		FString ErrorA;
		FString ErrorB;
		TestTrue(TEXT("First exact base generates"), GeneratorA->GenerateSection(Coordinate, BaseA, ErrorA));
		TestTrue(TEXT("Second exact base generates"), GeneratorB->GenerateSection(Coordinate, BaseB, ErrorB));
		TestEqual(TEXT("Exact base cell count is stable"), BaseA.Num(), BaseB.Num());
		TestEqual(TEXT("Exact base fingerprint is stable"), VoxelTest::HashBlocks(BaseA), VoxelTest::HashBlocks(BaseB));
	}
	FVoxelBlockState SampleA;
	FVoxelBlockState SampleB;
	FString SampleError;
	TestTrue(TEXT("Negative coordinate first query"), GeneratorA->SampleBlock(FIntVector(-17, 31, -9), SampleA, SampleError));
	TestTrue(TEXT("Negative coordinate second query"), GeneratorB->SampleBlock(FIntVector(-17, 31, -9), SampleB, SampleError));
	TestEqual(TEXT("Query bounds do not change natural cell"), SampleA.Pack(), SampleB.Pack());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelLandformEnvironmentTest,
	"WHFramework.Voxel.Generation.LandformEnvironment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelLandformEnvironmentTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const TSharedRef<FVoxelGenerationPipeline, ESPMode::ThreadSafe> First = VoxelTest::MakeGenerator(173);
	const TSharedRef<FVoxelGenerationPipeline, ESPMode::ThreadSafe> Second = VoxelTest::MakeGenerator(173);
	TArray<FVoxelEnvironmentSample> Grid;
	FString GridError;
	if (!TestTrue(TEXT("Natural environment grid resolves"),
		First->SampleEnvironments(FIntPoint(-33, 29), 3, 2, 16, Grid, GridError)))
	{
		AddError(GridError);
		return false;
	}
	TestEqual(TEXT("Natural environment grid count"), Grid.Num(), 6);
	for (int32 Index = 0; Index < Grid.Num(); ++Index)
	{
		const FIntPoint Position(-33 + (Index % 3) * 16,
			29 + (Index / 3) * 16);
		FVoxelEnvironmentSample Single;
		FString Error;
		if (!TestTrue(TEXT("Natural environment single resolves"),
			Second->SampleEnvironment(Position.X, Position.Y, Single, Error)))
		{
			AddError(Error);
			return false;
		}
		TestEqual(TEXT("Grid preserves natural height"),
			Grid[Index].Column.SurfaceZ, Single.Column.SurfaceZ);
		TestEqual(TEXT("Grid preserves natural landform"),
			Grid[Index].Column.Landform.Dominant, Single.Column.Landform.Dominant);
		TestEqual(TEXT("Grid preserves natural water"),
			Grid[Index].Column.SurfaceWaterZ, Single.Column.SurfaceWaterZ);
		TestEqual(TEXT("Grid preserves river distance"),
			Grid[Index].Column.RiverDistanceCells, Single.Column.RiverDistanceCells);
		TestTrue(TEXT("Grid preserves river identity"),
			Grid[Index].Column.RiverId == Single.Column.RiverId);
	}
	TArray<FVoxelEnvironmentSample> InvalidGrid;
	TestFalse(TEXT("Grid rejects exclusive-bound overflow"),
		First->SampleEnvironments(FIntPoint(MAX_int32, 0), 1, 1, 1,
			InvalidGrid, GridError));
	for (const FIntPoint Point : { FIntPoint(0, 0), FIntPoint(-8193, 2048), FIntPoint(24000, -17000) })
	{
		FVoxelEnvironmentSample A;
		FVoxelEnvironmentSample B;
		FString Error;
		if (!TestTrue(TEXT("First environment resolves"), First->SampleEnvironment(Point.X, Point.Y, A, Error)) ||
			!TestTrue(TEXT("Second environment resolves"), Second->SampleEnvironment(Point.X, Point.Y, B, Error)))
		{
			return false;
		}
		const FVoxelLandformSample& Shape = A.Column.Landform;
		const int32 WeightSum = Shape.PlainQ15 + Shape.HillsQ15 + Shape.HighlandQ15 +
			Shape.MountainQ15 + Shape.PlateauQ15 + Shape.BasinQ15;
		TestEqual(TEXT("Landform weights sum to Q15"), WeightSum, 32767);
		TestEqual(TEXT("Natural height is deterministic"), A.Column.SurfaceZ, B.Column.SurfaceZ);
		TestEqual(TEXT("Landform identity is deterministic"), Shape.Dominant, B.Column.Landform.Dominant);
		FVoxelColumnSample Column;
		if (!TestTrue(TEXT("Final column resolves"), First->SampleColumn(Point.X, Point.Y, Column, Error)))
		{
			return false;
		}
		TestEqual(TEXT("Final query keeps natural landform"), Shape.Dominant, Column.Landform.Dominant);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelSurfaceCandidateSupportTest,
	"WHFramework.Voxel.Generation.SurfaceCandidateSupport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSurfaceCandidateSupportTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationRuntimeConfig RuntimeConfig = *VoxelTest::MakeGenerationConfig();
	FVoxelGenerationRecipe Recipe = *RuntimeConfig.Recipe;
	Recipe.Settings.ContinentalAmplitude = 0;
	Recipe.Settings.MountainAmplitude = 0;
	Recipe.Settings.DetailAmplitude = 0;
	Recipe.Settings.Landform.PlainRelief = 0;
	Recipe.Settings.Landform.HillRelief = 0;
	Recipe.Settings.Landform.HillsDetailRelief = 0;
	Recipe.Settings.Landform.HighlandUplift = 0;
	Recipe.Settings.Landform.MountainBaseUplift = 0;
	Recipe.Settings.Landform.PlateauUplift = 0;
	Recipe.Settings.Landform.BasinDepth = 0;
	Recipe.Settings.Landform.ValleyDepth = 0;
	Recipe.Settings.SeaLevel = -64;
	Recipe.Settings.RiverSourceAccumulation = MAX_int32;
	RuntimeConfig.Recipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe));
	const TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config =
		MakeShared<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>(MoveTemp(RuntimeConfig));
	const TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelGenerationQuery Query;
	FString Error;
	if (!TestTrue(TEXT("Create surface query"), FVoxelGenerationQuery::Create(Config, Cache, Query, Error)))
	{
		return false;
	}

	const FVoxelGenerationPipeline Generator(Config, Cache);
	int32 Checked = 0;
	for (const FIntPoint XY : { FIntPoint(0, 0), FIntPoint(16, 16), FIntPoint(-16, -16) })
	{
		FVoxelSurfaceCandidate Candidate;
		if (!TestTrue(TEXT("Resolve candidate"), Query.ResolveSurfaceCandidate(XY.X, XY.Y, Candidate, Error)))
		{
			return false;
		}
		if (!Candidate.bValid || Candidate.bRiver || Candidate.bLake || Candidate.bOcean)
		{
			continue;
		}
		FVoxelBlockState Support;
		FVoxelBlockState Above;
		FVoxelColumnSample Column;
		if (!TestTrue(TEXT("Sample ground column"), Query.SampleEnvironmentColumn(XY.X, XY.Y, Column, Error)))
		{
			return false;
		}
		TestEqual(TEXT("Candidate and coarse terrain share the highest solid cell"), Candidate.GroundZ, Column.SurfaceZ);
		if (!TestTrue(TEXT("Sample support"), Generator.SampleBlock(FIntVector(XY.X, XY.Y, Candidate.GroundZ), Support, Error)) ||
			!TestTrue(TEXT("Sample space above support"), Generator.SampleBlock(FIntVector(XY.X, XY.Y, Candidate.GroundZ + 1), Above, Error)))
		{
			return false;
		}
		TestFalse(TEXT("Spawn support is terrain rather than air"), Support.IsAir());
		TestTrue(TEXT("Space directly above natural terrain is air"), Above.IsAir());
		++Checked;
	}
	TestTrue(TEXT("Checked at least one dry surface"), Checked > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelColumnSymbolsTest, "WHFramework.Voxel.Generation.ColumnSymbolsMatchPointQueries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelColumnSymbolsTest::RunTest(const FString& InParameters)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelGenerationQuery Query;
	FString Error;
	if (!TestTrue(TEXT("Query initializes"), FVoxelGenerationQuery::Create(Config, Cache, Query, Error))) return false;
	for (const FIntPoint Column : { FIntPoint(-17, 31), FIntPoint(0, 0), FIntPoint(127, -64) })
	{
		FVoxelColumnSample Surface;
		if (!TestTrue(TEXT("Column prepares"), Query.PrepareColumns({ FIntVector(Column.X, Column.Y, -128), FIntVector(Column.X + 1, Column.Y + 1, 512) }, Error))) return false;
		if (!TestTrue(TEXT("Surface resolves"), Query.SampleColumn(Column.X, Column.Y, Surface, Error))) return false;
		const int32 MinZ = Surface.SurfaceZ - 24;
		if (!TestTrue(TEXT("Full generation stages prepare"), Query.Prepare({ FIntVector(Column.X, Column.Y, MinZ), FIntVector(Column.X + 1, Column.Y + 1, MinZ + 48) }, Error))) return false;
		TArray<uint32> Symbols;
		if (!TestTrue(TEXT("Column symbols resolve"), Query.SampleColumnSymbols(Column, MinZ, 48, Symbols, Error))) return false;
		for (int32 Index = 0; Index < Symbols.Num(); ++Index)
		{
			uint32 Point = 0;
			TestTrue(TEXT("Point query resolves"), Query.SampleSymbol(FIntVector(Column.X, Column.Y, MinZ + Index), Point, Error));
			TestEqual(TEXT("Column batching preserves full generation semantics"), Symbols[Index], Point);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPlanBuildDiagnosticsTest,
	"WHFramework.Voxel.Generation.PlanBuildDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPlanBuildDiagnosticsTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationPlanCache Cache(true, false);
	FVoxelHydrologyPlanPtr Plan;
	FString Error;
	int32 Builds = 0;
	const auto Build = [&Builds](FVoxelHydrologyPlan&, FString&) { ++Builds; return true; };
	TestTrue(TEXT("Cold cache builds an immutable plan"), Cache.GetOrBuildHydrology({{0, 0}}, Build, Plan, Error));
	TestTrue(TEXT("Warm cache reuses the immutable plan"), Cache.GetOrBuildHydrology({{0, 0}}, Build, Plan, Error));
	TestEqual(TEXT("A hit never calls the builder"), Builds, 1);
	TestFalse(TEXT("Failed generation is recorded separately"), Cache.GetOrBuildHydrology({{1, 0}},
		[](FVoxelHydrologyPlan&, FString& OutError) { OutError = TEXT("Invalid test plan"); return false; }, Plan, Error));
	const auto Stats = Cache.GetPlanDiagnostics(EVoxelGenerationPlanKind::Hydrology);
	TestEqual(TEXT("Statistics count actual builder executions"), Stats.BuildCount, 2ull);
	TestEqual(TEXT("Statistics count warm cache hits"), Stats.HitCount, 1ull);
	TestEqual(TEXT("Statistics count cold misses"), Stats.MissCount, 2ull);
	TestEqual(TEXT("Statistics count actual failures"), Stats.FailedCount, 1ull);
	TestTrue(TEXT("Build durations are finite and independently aggregated"),
		FMath::IsFinite(Stats.TotalBuildMilliseconds) && Stats.TotalBuildMilliseconds >= Stats.MaximumBuildMilliseconds);
	return true;
}

#endif
