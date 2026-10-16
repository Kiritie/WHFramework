#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"
#include "Voxel/Authoring/VoxelWorldGenerationProfile.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationRecipeCodec.h"
#include "Voxel/Tests/VoxelTestUtilities.h"
#include "Voxel/Generation/VoxelGenerationPlanningRead.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "Voxel/Generation/VoxelGenerationPlanCoordinator.h"
#include "Voxel/Generation/Climate/VoxelClimateGenerator.h"
#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyMacroDrySeaPlaneTest,
	"WHFramework.Voxel.Hydrology.Macro.SeaPlaneNeedsPositiveDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyMacroDrySeaPlaneTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationRecipe Recipe = *VoxelTest::MakeGenerationConfig()->Recipe;
	Recipe.Settings.BaseHeight = 0;
	Recipe.Settings.SeaLevel = 0;
	Recipe.Settings.ContinentalAmplitude = 0;
	Recipe.Settings.MountainAmplitude = 0;
	Recipe.Settings.DetailAmplitude = 0;
	Recipe.Settings.HydrologyRegionSide = 8;
	Recipe.Settings.HydrologyHaloCells = 8;
	Recipe.Settings.RiverSourceAccumulation = MAX_int32;
	auto& Shape = Recipe.Settings.Landform;
	Shape.PlainRelief = 0;
	Shape.HillRelief = 0;
	Shape.HillsDetailRelief = 0;
	Shape.HighlandUplift = 0;
	Shape.MountainBaseUplift = 0;
	Shape.PlateauUplift = 0;
	Shape.BasinDepth = 0;
	Shape.ValleyDepth = 0;
	const auto Frozen = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe));
	const auto Climate = MakeShared<const FVoxelClimateGenerator, ESPMode::ThreadSafe>(Frozen);
	const auto Terrain = MakeShared<const FVoxelTerrainGenerator, ESPMode::ThreadSafe>(Frozen, Climate);
	const FVoxelHydrologyGenerator Generator(Frozen, Terrain);
	FVoxelHydrologyPlan Plan;
	FString Error;
	if (!TestTrue(TEXT("真实宏观生成器构建海平面干地夹具"), Generator.BuildTopology({FIntPoint::ZeroValue}, Plan, Error)))
	{
		AddError(Error);
		return false;
	}
	if (!TestTrue(TEXT("夹具实际含有宏观格"), !Plan.Grid.GroundPlane.IsEmpty())) return false;
	for (int32 Index = 0; Index < Plan.Grid.GroundPlane.Num(); ++Index)
	{
		TestEqual(TEXT("精确构造等于海平面的宏观地面"), int32(Plan.Grid.GroundPlane[Index]), 0);
		TestFalse(TEXT("零水深格不登记为湿接收体"), Plan.Grid.OceanOutlets.Contains(Index));
	}
	const TMap<uint32, int32> EmptyOcean;
	TestEqual(TEXT("海平面干格不预留整区海洋哈希表"), Plan.Grid.OceanOutlets.GetAllocatedSize(), EmptyOcean.GetAllocatedSize());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyMacroBasinTest,
	"WHFramework.Voxel.Hydrology.Macro.CoarseBasinConservationAndResume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyMacroBasinTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationRecipe Recipe = *VoxelTest::MakeGenerationConfig()->Recipe;
	Recipe.Settings.HydrologyCellSize = 16;
	Recipe.Settings.HydrologyRegionSide = 8;
	Recipe.Settings.HydrologyHaloCells = 8;
	Recipe.Settings.SeaLevel = 0;
	Recipe.Settings.RiverBaseHalfWidth = 2;
	Recipe.Settings.RiverBaseDepth = 2;
	const auto Frozen = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe));
	const auto Climate = MakeShared<const FVoxelClimateGenerator, ESPMode::ThreadSafe>(Frozen);
	const auto Terrain = MakeShared<const FVoxelTerrainGenerator, ESPMode::ThreadSafe>(Frozen, Climate);
	TMap<FVoxelHydrologyRegionKey, FVoxelHydrologyPlan> Tiles;
	auto Lookup = [&](const FVoxelHydrologyRegionKey& Key) -> const FVoxelHydrologyPlan*
	{
		if (const auto* Existing = Tiles.Find(Key)) return Existing;
		auto& Tile = Tiles.Add(Key);
		Tile.Grid.WorldMinCell = Key.Coordinate * 8;
		Tile.Grid.Width = 8;
		Tile.Grid.Height = 8;
		Tile.Grid.CellSize = 16;
		Tile.Grid.GroundPlane.SetNum(64);
		Tile.Grid.RainWeight.Init(100, 64);
		for (int32 Index = 0; Index < 64; ++Index)
		{
			const FIntPoint Cell = Tile.Grid.ToWorldHydrologyCell(Index);
			const bool bLand = FMath::Abs(Cell.X) <= 1 && FMath::Abs(Cell.Y) <= 1;
			Tile.Grid.GroundPlane[Index] = bLand ? 4 : -4;
			if (!bLand) Tile.Grid.OceanOutlets.Add(Index, 0);
		}
		return &Tile;
	};
	FVoxelGenerationRuntimeConfig ComponentAuthor = *VoxelTest::MakeGenerationConfig();
    ComponentAuthor.Recipe = Frozen;
    const auto ComponentConfig = MakeShared<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>(MoveTemp(ComponentAuthor));
    FVoxelGenerationPlanCache FirstCache;
    FVoxelHydrologyComponentBuild First(ComponentConfig, FirstCache, FIntPoint::ZeroValue);
	FString Error;
	TAtomic<bool> Cancel(true);
	TestTrue(TEXT("宏观图在读取前支持取消"), First.Advance(Lookup, 1, Error, &Cancel) == EVoxelHydrologyComponentStep::Canceled);
	Cancel.Store(false);
	auto Finish = [&](FVoxelHydrologyComponentBuild& Build, const int32 Budget)
	{
		const double Deadline = FPlatformTime::Seconds() + 5.0;
		EVoxelHydrologyComponentStep Step;
		do
		{
			Step = Build.Advance(Lookup, Budget, Error, &Cancel);
			if (Step != EVoxelHydrologyComponentStep::Ready && Step != EVoxelHydrologyComponentStep::Yielded)
			{
				AddError(Error);
				return false;
			}
		} while (Step != EVoxelHydrologyComponentStep::Ready && FPlatformTime::Seconds() < Deadline);
		return Step == EVoxelHydrologyComponentStep::Ready;
	};
	if (!TestTrue(TEXT("逐节点推进真实宏观流域完成"), Finish(First, 1))) return false;
	const auto& Result = First.GetResult();
	TestTrue(TEXT("宏观流域实际输出河段"), !Result.Plan.Rivers.IsEmpty());
	TestTrue(TEXT("河网声明宏观权威并建立完整索引"), Result.Plan.bMacroAuthoritative && Result.Plan.bCompleteRiverIndex);
	TestTrue(TEXT("实际汇水守恒且非空"), Result.TotalRainWeight > 0 && Result.TotalRainWeight + Result.IncomingRainWeight == Result.ReceivedRainWeight + Result.ExportedRainWeight);
	for (const auto& River : Result.Plan.Rivers)
	{
		for (int32 Index = 1; Index < River.MeanderPoints.Num(); ++Index)
		{
			TestTrue(TEXT("局部曲线保持宏观水位单调"), River.MeanderPoints[Index - 1].WaterZ >= River.MeanderPoints[Index].WaterZ);
			TestTrue(TEXT("控制水位来自宏观事实"), River.MeanderPoints[Index].WaterZ <= 4);
		}
	}
	FVoxelGenerationPlanCache SecondCache;
    FVoxelHydrologyComponentBuild Second(ComponentConfig, SecondCache, FIntPoint::ZeroValue);
	if (!TestTrue(TEXT("不同工作片预算也完成同一流域"), Finish(Second, 17))) return false;
	TestTrue(TEXT("恢复批次不改变最终输出"), Result.HasEquivalentOutput(Second.GetResult()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyReceiverReadBoundsTest,
	"WHFramework.Voxel.Hydrology.Macro.ReceiverReadsRespectTopologyBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyReceiverReadBoundsTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationRecipe Recipe = *VoxelTest::MakeGenerationConfig()->Recipe;
	Recipe.Settings.HydrologyCellSize = 16;
	Recipe.Settings.HydrologyRegionSide = 8;
	Recipe.Settings.HydrologyHaloCells = 1;
	Recipe.Settings.SeaLevel = 0;
	const auto Frozen = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe));
	const auto Climate = MakeShared<const FVoxelClimateGenerator, ESPMode::ThreadSafe>(Frozen);
	const auto Terrain = MakeShared<const FVoxelTerrainGenerator, ESPMode::ThreadSafe>(Frozen, Climate);
	const auto CheckReads = [&](const FIntPoint& Seed, const int32 ExpectedRegions)
	{
		TMap<FVoxelHydrologyRegionKey, FVoxelHydrologyPlan> Tiles;
		TSet<FIntPoint> Requested;
		FString Error;
		const auto Lookup = [&](const FVoxelHydrologyRegionKey& Key) -> const FVoxelHydrologyPlan*
		{
			Requested.Add(Key.Coordinate);
			if (const auto* Existing = Tiles.Find(Key)) return Existing;
			auto& Tile = Tiles.Add(Key);
			Tile.Grid.WorldMinCell = Key.Coordinate * 8 - FIntPoint(1, 1);
			Tile.Grid.Width = 10;
			Tile.Grid.Height = 10;
			Tile.Grid.CellSize = 16;
			Tile.Grid.GroundPlane.Init(-4, 100);
			Tile.Grid.RainWeight.Init(1, 100);
			for (int32 Index = 0; Index < 100; ++Index) Tile.Grid.OceanOutlets.Add(Index, 0);
			if (Key.Coordinate == FIntPoint(-1, 0))
			{
				auto& Lake = Tile.Lakes.AddDefaulted_GetRef();
				Lake.Id = {7, 9};
				Lake.WaterZ = 10;
				Lake.Cells.Add(FIntPoint(0, 64));
			}
			if (!Tile.BuildSpatialIndex(Error)) return nullptr;
			return &Tile;
		};
		FVoxelHydrologyRegionSample Sample;
        FIntPoint World;
        const auto Request = [&](const FVoxelHydrologyRegionKey&, FString& InError)
        {
            InError = TEXT("Unexpected missing topology in direct receiver fixture");
            return false;
        };
        const auto Read = FVoxelHydrologyMacroInput::Read(Frozen->Settings, Seed, Lookup, Request, Sample, World, Error);
        if (!TestTrue(TEXT("正式共享输入在有界接收体中完成"), Read == EVoxelHydrologyClosureRead::Ready))
        {
            AddError(Error);
            return false;
        }
        TestTrue(TEXT("完整保留接收体正水深"), Sample.IsReceiver() && Sample.WaterZ > Sample.GroundZ);
		if (!TestEqual(TEXT("只请求覆盖实际粗格的接收体区域"), Requested.Num(), ExpectedRegions)) return false;
		if (ExpectedRegions == 2)
		{
			if (!TestTrue(TEXT("跨边界湖泊所属区域仍被读取"), Requested.Contains(FIntPoint(-1, 0)))) return false;
			const auto* Lake = Tiles.FindChecked({FIntPoint(-1, 0)}).FindLakeAtOrigin(FIntPoint(0, 64));
			TestTrue(TEXT("跨区湖泊保留原控制水位与稳定身份"), Lake && Lake->WaterZ == 10 && Lake->Id.High == 7 && Lake->Id.Low == 9);
		}
		return true;
	};
	const bool Interior = CheckReads(FIntPoint(4, 4), 1);
	const bool Boundary = CheckReads(FIntPoint(0, 4), 2);
	return Interior && Boundary;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyLocalShoreTest,
	"WHFramework.Voxel.Hydrology.Macro.LocalShoreKeepsFineGround",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyLocalShoreTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelHydrologyPlan Plan;
	Plan.Grid.WorldMinCell = FIntPoint(-1, -1);
	Plan.Grid.Width = 1;
	Plan.Grid.Height = 1;
	Plan.Grid.CellSize = 8;
	Plan.Grid.OceanOutlets.Add(0, 10);
	FVoxelHydrologyInfluence Influence;
	TestTrue(TEXT("宏观海体内低列有实际海水"), Plan.SampleBaseWater(-1, -1, 3, Influence));
	TestTrue(TEXT("海水保持宏观身份与控制水位"), Influence.bOcean && Influence.SurfaceWaterZ == 10);
	TestEqual(TEXT("海床使用实际精细地面"), Influence.GroundOverrideZ, 3);
	for (const int32 Ground : {10, 13})
	{
		Plan.SampleBaseWater(-1, -1, Ground, Influence);
		TestTrue(TEXT("宏观海体中的高列形成干岸"), Influence.bCoast && !Influence.bOcean && !Influence.bLake);
		TestEqual(TEXT("干岸没有虚构水体"), Influence.SurfaceWaterZ, MIN_int32);
		TestEqual(TEXT("不压低精细地面来制造水域"), Influence.GroundOverrideZ, MIN_int32);
	}
	Plan.Grid.OceanOutlets.Reset();
	FVoxelLakePlan& Lake = Plan.Lakes.AddDefaulted_GetRef();
	Lake.WaterZ = 10;
	Lake.Cells.Add(FIntPoint(-8, -8));
	FString Error;
	if (!TestTrue(TEXT("湖体通过正式入口建立空间索引"), Plan.BuildSpatialIndex(Error))) return false;
	TestTrue(TEXT("宏观湖体内低列有实际湖水"), Plan.SampleBaseWater(-1, -1, 3, Influence));
	TestTrue(TEXT("湖水保持控制水位"), Influence.bLake && !Influence.bOcean && Influence.SurfaceWaterZ == 10);
	TestEqual(TEXT("湖床使用实际精细地面"), Influence.GroundOverrideZ, 3);
	TestFalse(TEXT("宏观湖体中的高列保持干地"), Plan.SampleBaseWater(-1, -1, 13, Influence));
	TestEqual(TEXT("高列没有河床覆盖"), Influence.GroundOverrideZ, MIN_int32);
	TestEqual(TEXT("高列没有湖水"), Influence.SurfaceWaterZ, MIN_int32);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyMacroRuntimeTest,
	"WHFramework.Voxel.Hydrology.Owner.MacroRuntimeResume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyMacroRuntimeTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationRuntimeConfig Author = *VoxelTest::MakeGenerationConfig(173, FVoxelGenerationRecipe::CurrentAlgorithmVersion);
	FVoxelGenerationRecipe Recipe = *Author.Recipe;
	Recipe.Settings.BaseHeight = -32;
	Recipe.Settings.HydrologyCellSize = 64;
	Recipe.Settings.HydrologyRegionSide = 8;
	Recipe.Settings.HydrologyHaloCells = 8;
	Recipe.Settings.MaxZ = 2048;
	Recipe.Settings.SeaLevel = Recipe.Settings.MaxZ - 1;
	FString Error;
	if (!TestTrue(TEXT("宏观水文受控配方使用真实编码哈希"), FVoxelGenerationRecipeCodec::RefreshHash(Recipe, Error))) return false;
	Author.Recipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe));
	const auto Consumer = FVoxelGenerationConsumerKey::Section(105, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	if (!TestTrue(TEXT("真实账本冻结宏观水文配方并创建缓存"), FVoxelTerrainResources::Create({}, Consumer, Resources, Error) &&
		FVoxelGenerationBinding::Freeze(Author, Resources.ToSharedRef(), Consumer, Config, Error) &&
		FVoxelGenerationPlanCache::Create(Resources.ToSharedRef(), Consumer, Cache, Error, true, false))) return false;
	FVoxelGenerationConsumerScope Identity(Consumer);
	FVoxelGenerationQuery Query;
	if (!TestTrue(TEXT("宏观水文查询接纳真实资源"), FVoxelGenerationQuery::Create(Config.ToSharedRef(), Cache.ToSharedRef(), Query,
		Error, false, false, false, &Consumer))) return false;
	const FIntPoint Cell(-1, -1);
	FVoxelHydrologyPlanPtr Coverage;
	const FVoxelHydrologyRegionKey Region = FVoxelGenerationQuery::HydrologyKeyForVoxel(Cell.X, Cell.Y,
		Config->Recipe->Settings, Config->Recipe->AlgorithmVersion);
	TAtomic<bool> Cancel(true);
	TestFalse(TEXT("取消不发布宏观计划"), Query.EnsureHydrologyPlan(Region, Coverage, Error, &Cancel));
	TestEqual(TEXT("取消状态明确"), Error, FString(TEXT("Canceled")));
	Cancel.Store(false);
	FVoxelGenerationCacheRetention Retention;
	Retention.Revision = 1;
	Cache->UpdateRetention(Retention);
	if (!TestTrue(TEXT("真实求解先进入可续跑状态"),
		!Query.EnsureHydrologyPlan(Region, Coverage, Error) && Error.StartsWith(TEXT("Yielded")))) return false;
	const FVoxelHydrologyRegionKey Remote{FIntPoint(4096, 4096)};
	FVoxelHydrologyPlanPtr RemotePlan;
	int32 RemoteBuilds = 0;
	const auto BuildRemote = [&]()
	{
		return Cache->GetOrBuildHydrologyTopology(Remote, [&](FVoxelHydrologyPlan& Output, FString& BuildError)
		{
			++RemoteBuilds;
			Output.Key = Remote;
			Output.Grid.WorldMinCell = Remote.Coordinate * 8;
			Output.Grid.Width = 8;
			Output.Grid.Height = 8;
			Output.Grid.CellSize = 64;
			BuildError.Reset();
			return true;
		}, RemotePlan, Error);
	};
	const auto Maintain = [&]()
	{
		for (int32 Index = 0; Index < FVoxelGenerationPlanCache::ShardCount * 2; ++Index)
		{
			Cache->TickMaintenance(256);
		}
	};
	const auto AwaitRemoteRetirement = [&]()
	{
		const double Deadline = FPlatformTime::Seconds() + 2.0;
		do
		{
			Maintain();
			FVoxelHydrologyPlanPtr Probe;
			if (!Cache->FindHydrologyTopology(Remote, Probe))
			{
				return true;
			}
			Probe.Reset();
			FPlatformProcess::Sleep(0.001f);
		} while (FPlatformTime::Seconds() < Deadline);
		return false;
	};
	if (!TestTrue(TEXT("缓存实际发布无当前读者的远端拓扑"), BuildRemote())) return false;
	RemotePlan.Reset();
	Maintain();
	if (!TestTrue(TEXT("活动求解跨工作片复用远端拓扑"),
		Cache->FindHydrologyTopology(Remote, RemotePlan))) return false;
	RemotePlan.Reset();
	TestTrue(TEXT("远端热读无需重建"), BuildRemote());
	TestEqual(TEXT("低压力下远端拓扑仅构建一次"), RemoteBuilds, 1);
	RemotePlan.Reset();
	const auto State = Resources->Snapshot();
	const uint64 Used = State.Used[int32(EVoxelTerrainResourcePool::Resident)];
	const uint64 NativeLimit = State.Limits.ResidentBytes - State.Limits.OverlapBytes;
	if (!TestTrue(TEXT("真实账本仍有可用于压力测试的容量"), Used + 1 < NativeLimit)) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Pressure;
	if (!TestTrue(TEXT("用原账本制造容量压力而不提高上限"), Resources->Acquire(Consumer,
		EVoxelTerrainResourcePool::Resident, NativeLimit - Used - 1, Pressure))) return false;
	TestTrue(TEXT("压力维护等待原构建门闩的异步持有者退出"), AwaitRemoteRetirement());
	TestFalse(TEXT("活动求解不阻止压力下回收无读者远端拓扑"),
		Cache->FindHydrologyTopology(Remote, RemotePlan));
	Pressure.Reset();
	if (!TestTrue(TEXT("压力解除后可重新发布远端输入"), BuildRemote())) return false;
	RemotePlan.Reset();
	Cache->CancelHydrologyBuild(Region, Consumer.WorldEpoch, Consumer.OwnerId,
		Consumer.OwnerIncarnation, Consumer.RequestSerial);
	TestTrue(TEXT("取消维护等待最后异步读者退出后回收远端输入"), AwaitRemoteRetirement());
	TestFalse(TEXT("取消原构建后远端输入恢复空间退休"),
		Cache->FindHydrologyTopology(Remote, RemotePlan));
	bool bReady = false;
	const double CoverageDeadline = FPlatformTime::Seconds() + 10.0;
	do
	{
		bReady = Query.EnsureHydrologyPlan(Region, Coverage, Error);
		if (!bReady && !Error.StartsWith(TEXT("Yielded")))
		{
			AddError(Error);
			return false;
		}
	} while (!bReady && FPlatformTime::Seconds() < CoverageDeadline);
	if (!TestTrue(TEXT("真实宏观水文覆盖计划经恢复完成"), bReady && Coverage.IsValid())) return false;
	if (!TestTrue(TEXT("覆盖消费实际发布的宏观输入"), Coverage->TopologyPlan.IsValid())) return false;
	const auto& Topology = *Coverage->TopologyPlan;
	TestTrue(TEXT("宏观输入不保留完整排水求解正文"), Topology.Drainage.Parent.IsEmpty() &&
		Topology.Drainage.SpillPlane.IsEmpty() && Topology.Drainage.Accumulation.IsEmpty() && Topology.Drainage.SettledOrder.IsEmpty());
	TestTrue(TEXT("宏观输入不提前生成重复河段曲线"), Topology.Rivers.IsEmpty() && Topology.Waterfalls.IsEmpty());
	TestTrue(TEXT("临时湖泊构建掩码与干出口不常驻"), Topology.Grid.Allowed.IsEmpty() && Topology.Grid.CanonicalSinkOutlets.IsEmpty());
	FVoxelHydrologyPlanPtr Warm;
	TestTrue(TEXT("热读复用同一不可变覆盖事实"), Query.EnsureHydrologyPlan(Region, Warm, Error) && Warm == Coverage);
	FVoxelColumnSample Column;
	if (!TestTrue(TEXT("正式自然列读取只消费宏观水文覆盖事实"),
		Query.SampleEnvironmentColumn(Cell.X, Cell.Y, Column, Error))) return false;
	TestTrue(TEXT("消费保持海水身份及正水深"), Column.bOcean && Column.SurfaceWaterZ > Column.SurfaceZ);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> GeometryResources;
	TUniquePtr<FVoxelTaskScheduler> SchedulerOwner;
	if (!TestTrue(TEXT("调度器复用同一事实账本和世界身份"),
		FVoxelTerrainResources::Create({}, Consumer, GeometryResources, Error) &&
		FVoxelTaskScheduler::Create(GeometryResources.ToSharedRef(), Resources.ToSharedRef(), Consumer, SchedulerOwner, Error) &&
		SchedulerOwner->InitializeStatistics(105))) return false;
	FVoxelTaskScheduler& Scheduler = *SchedulerOwner;
	FVoxelGenerationPlanCoordinator Coordinator(Scheduler, Config.ToSharedRef(), Cache.ToSharedRef(), 105);
	FVoxelGenerationPlanDependencyCapture Capture;
	if (!TestTrue(TEXT("负坐标稀疏采样只登记宏观覆盖依赖"), Coordinator.GatherEnvironmentSamples(Cell, 1, 1, 1,
		Consumer, Capture, Error))) return false;
	if (!TestEqual(TEXT("单点捕获一个宏观区域"), Capture.Values.Num(), 1)) return false;
	TestTrue(TEXT("依赖直接指向宏观水文"), Capture.Values[0].Kind == EVoxelGenerationPlanKind::Hydrology);
	FVoxelGenerationDependencyStatus Status;
	const double CoordinatorDeadline = FPlatformTime::Seconds() + 10.0;
	do
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		Status = Coordinator.Ensure(Capture.Values, EVoxelWorkClass::Critical, 0, 0.0, 0.0, Consumer);
		if (!Status.bReady && !Status.bFailed) FPlatformProcess::Sleep(0.001f);
	} while (!Status.bReady && !Status.bFailed && FPlatformTime::Seconds() < CoordinatorDeadline);
	TestTrue(TEXT("实际调度器接受已发布宏观覆盖"), Status.bReady);
	TestFalse(TEXT("实际调度器没有失败事实"), Status.bFailed);
	if (!Status.bReady) AddError(Status.Error);
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyRuntimeVersionTest,
	"WHFramework.Voxel.Hydrology.Owner.RuntimeVersionBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyRuntimeVersionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelRegistrySnapshot Registry = *VoxelTest::MakeRegistry();
	FVoxelGenerationRecipe Recipe = *VoxelTest::MakeGenerationConfig()->Recipe;
	for (int32 Index = 1; Index < Recipe.BlockNames.Num(); ++Index)
	{
		Recipe.BlockNames[Index] = Registry.Definitions[Index].BlockName;
	}
	for (uint32 Version = 0; Version <= FVoxelGenerationRecipe::CurrentAlgorithmVersion + 1; ++Version)
	{
        if (Version == FVoxelGenerationRecipe::CurrentAlgorithmVersion) continue;
		FVoxelGenerationRecipe Retired = Recipe;
		Retired.AlgorithmVersion = Version;
		FString Error;
		TArray<uint8> Bytes;
		TestFalse(TEXT("非当前算法不能重新编码"), FVoxelGenerationRecipeCodec::Encode(Retired, Bytes, Error));
		FVoxelGenerationPlanCache Cache;
		TestFalse(TEXT("非当前算法不能绑定运行缓存"), Cache.BindHydrologyRecipe(100, Version, Error));
	}
	FString Error;
	if (!TestTrue(TEXT("当前算法生成真实编码身份"), FVoxelGenerationRecipeCodec::RefreshHash(Recipe, Error))) return false;
	UVoxelWorldGenerationProfile* Profile = NewObject<UVoxelWorldGenerationProfile>();
	Profile->RecipeHash = Recipe.RecipeHash;
	Profile->BakedCellCentimeters = Recipe.CellCentimeters;
	if (!TestTrue(TEXT("当前算法配方编码"), FVoxelGenerationRecipeCodec::Encode(Recipe, Profile->RecipeBytes, Error))) return false;
	FVoxelGenerationRuntimeConfig Config;
	if (!TestTrue(TEXT("当前算法允许实际资产运行绑定"), FVoxelGenerationBinding::Build(*Profile, Registry,
		Recipe.Settings, Recipe.CellCentimeters, Config, Error))) return false;
	TestEqual(TEXT("运行绑定保持当前算法"), Config.Recipe->AlgorithmVersion, FVoxelGenerationRecipe::CurrentAlgorithmVersion);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyOwnerRecipeTest,
	"WHFramework.Voxel.Hydrology.Owner.RecipeLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyOwnerRecipeTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationPlanCache First;
	FString Error;
	TestTrue(TEXT("Old recipe binds to its own cache"), First.BindHydrologyRecipe(100, FVoxelGenerationRecipe::CurrentAlgorithmVersion, Error));
	First.Reset();
	TestTrue(TEXT("Same recipe may create fresh queries after content reset"), First.BindHydrologyRecipe(100, FVoxelGenerationRecipe::CurrentAlgorithmVersion, Error));
	TestFalse(TEXT("Reset cannot change the original recipe identity"), First.BindHydrologyRecipe(200, FVoxelGenerationRecipe::CurrentAlgorithmVersion, Error));
	TestTrue(TEXT("Recipe mismatch gives a lifecycle diagnosis"), Error.StartsWith(TEXT("InvalidRecipe")));
	FVoxelGenerationPlanCache Second;
	TestTrue(TEXT("New recipe binds to a new cache"), Second.BindHydrologyRecipe(200, FVoxelGenerationRecipe::CurrentAlgorithmVersion, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyOwnerGenerationTest,
	"WHFramework.Voxel.Hydrology.Owner.ResetGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyOwnerGenerationTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationPlanCache Cache;
	FString Error;
	const uint64 Original = Cache.GetHydrologyGeneration();
	TestTrue(TEXT("Original query generation binds"), Cache.BindHydrologyRecipe(100, FVoxelGenerationRecipe::CurrentAlgorithmVersion, Error, Original));
	Cache.Reset();
	const uint64 Current = Cache.GetHydrologyGeneration();
	TestTrue(TEXT("Reset advances the generation"), Current > Original);
	TestFalse(TEXT("Stale query cannot bind or read a replacement generation"), Cache.BindHydrologyRecipe(100, FVoxelGenerationRecipe::CurrentAlgorithmVersion, Error, Original));
	TestEqual(TEXT("Stale query is canceled"), Error, FString(TEXT("Canceled")));
	TestTrue(TEXT("Current same-recipe query can resume"), Cache.BindHydrologyRecipe(100, FVoxelGenerationRecipe::CurrentAlgorithmVersion, Error, Current));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyOwnerVersionTest,
	"WHFramework.Voxel.Hydrology.Owner.VersionGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyOwnerVersionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationPlanCache Cache;
	FString Error;
	TestFalse(TEXT("Unknown future version cannot bind"), Cache.BindHydrologyRecipe(999,
		FVoxelGenerationRecipe::CurrentAlgorithmVersion + 1, Error));
	TestTrue(TEXT("Rejected version does not poison the empty cache"), Cache.BindHydrologyRecipe(100, FVoxelGenerationRecipe::CurrentAlgorithmVersion, Error));
	return true;
}

#endif
