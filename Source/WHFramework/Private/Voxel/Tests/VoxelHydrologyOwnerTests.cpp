#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyFineWaterRuntimeTest,
	"WHFramework.Voxel.Hydrology.Owner.FineWaterRuntimeResume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyFineWaterRuntimeTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationRuntimeConfig Author = *VoxelTest::MakeGenerationConfig(173, FVoxelGenerationRecipe::CurrentAlgorithmVersion);
	FVoxelGenerationRecipe Recipe = *Author.Recipe;
	Recipe.Settings.BaseHeight = -32;
	Recipe.Settings.MaxZ = 2048;
	Recipe.Settings.SeaLevel = Recipe.Settings.MaxZ - 1;
	FString Error;
	if (!TestTrue(TEXT("v14受控配方使用真实编码哈希"), FVoxelGenerationRecipeCodec::RefreshHash(Recipe, Error))) return false;
	Author.Recipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe));
	const auto Consumer = FVoxelGenerationConsumerKey::Section(105, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	if (!TestTrue(TEXT("真实账本冻结v14配方并创建缓存"), FVoxelTerrainResources::Create({}, Consumer, Resources, Error) &&
		FVoxelGenerationBinding::Freeze(Author, Resources.ToSharedRef(), Consumer, Config, Error) &&
		FVoxelGenerationPlanCache::Create(Resources.ToSharedRef(), Consumer, Cache, Error, true, false))) return false;
	FVoxelGenerationConsumerScope Identity(Consumer);
	FVoxelGenerationQuery Query;
	if (!TestTrue(TEXT("v14查询接纳真实资源"), FVoxelGenerationQuery::Create(Config.ToSharedRef(), Cache.ToSharedRef(), Query,
		Error, false, false, false, &Consumer))) return false;
	FVoxelHydrologyFinePlanPtr Fine;
	const FIntPoint Cell(-1, -1);
	const auto Climate = MakeShared<const FVoxelClimateGenerator, ESPMode::ThreadSafe>(Config->Recipe.ToSharedRef());
	const FVoxelTerrainGenerator Terrain(Config->Recipe.ToSharedRef(), Climate);
	if (!TestTrue(TEXT("受控海体输入确实低于水面"), Terrain.SampleSurfaceHeight(Cell.X, Cell.Y) < Config->Recipe->Settings.SeaLevel)) return false;
	TAtomic<bool> Cancel(true);
	TestFalse(TEXT("取消不发布细水事实"), Query.EnsureFineWater(Cell, Fine, Error, &Cancel));
	TestEqual(TEXT("取消状态明确"), Error, FString(TEXT("Canceled")));
	Cancel.Store(false);
	bool bReady = false;
	int32 Yields = 0;
	const double Deadline = FPlatformTime::Seconds() + 10.0;
	do
	{
		bReady = Query.EnsureFineWater(Cell, Fine, Error, &Cancel);
		if (!bReady && !Error.StartsWith(TEXT("Yielded")))
		{
			AddError(Error);
			return false;
		}
		Yields += !bReady ? 1 : 0;
	} while (!bReady && FPlatformTime::Seconds() < Deadline);
	if (!TestTrue(TEXT("合作式缓存构建真实完成"), bReady && Fine.IsValid())) return false;
	const auto Water = Fine->SampleWater(Cell);
	TestTrue(TEXT("实际海接收体保留正水深"), Water.IsWet() && Water.WaterZ > Terrain.SampleMacro(Cell.X, Cell.Y).SurfaceZ);
	TestEqual(TEXT("实际水体固定水位"), Water.WaterZ, Config->Recipe->Settings.SeaLevel);
	FVoxelHydrologyFinePlanPtr Warm;
	TestTrue(TEXT("热读复用同一不可变事实"), Query.EnsureFineWater(Cell, Warm, Error) && Warm == Fine);
	AddInfo(FString::Printf(TEXT("v14真实细水恢复：yields=%d bytes=%llu"), Yields, Fine->GetAllocatedBytes()));
	FVoxelHydrologyPlanPtr Coverage;
	const FVoxelHydrologyRegionKey Region = FVoxelGenerationQuery::HydrologyKeyForVoxel(Cell.X, Cell.Y,
		Config->Recipe->Settings, Config->Recipe->AlgorithmVersion);
	bReady = false;
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
	if (!TestTrue(TEXT("真实v14覆盖计划经恢复完成"), bReady && Coverage.IsValid())) return false;
	FVoxelColumnSample Column;
	if (!TestTrue(TEXT("正式自然列读取消费v14细水和覆盖事实"),
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
	if (!TestTrue(TEXT("负坐标稀疏采样生成细水依赖"), Coordinator.GatherEnvironmentSamples(Cell, 1, 1, 1,
		Consumer, Capture, Error))) return false;
	FVoxelGenerationDependencyStatus Status;
	const double CoordinatorDeadline = FPlatformTime::Seconds() + 10.0;
	do
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		Status = Coordinator.Ensure(Capture.Values, EVoxelWorkClass::Critical, 0, 0.0, 0.0, Consumer);
		if (!Status.bReady && !Status.bFailed) FPlatformProcess::Sleep(0.001f);
	} while (!Status.bReady && !Status.bFailed && FPlatformTime::Seconds() < CoordinatorDeadline);
	TestTrue(TEXT("实际调度器接受已认证细水"), Status.bReady);
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
	TArray<uint64> Hashes;
	for (const uint32 Version : { uint32(12), uint32(13), uint32(14) })
	{
		FVoxelGenerationRecipe Recipe = *VoxelTest::MakeGenerationConfig()->Recipe;
		Recipe.AlgorithmVersion = Version;
		for (int32 Index = 1; Index < Recipe.BlockNames.Num(); ++Index)
		{
			Recipe.BlockNames[Index] = Registry.Definitions[Index].BlockName;
		}
		FString Error;
		if (!TestTrue(TEXT("版本配方哈希计算"), FVoxelGenerationRecipeCodec::RefreshHash(Recipe, Error))) return false;
		UVoxelWorldGenerationProfile* Profile = NewObject<UVoxelWorldGenerationProfile>();
		Profile->RecipeHash = Recipe.RecipeHash;
		Profile->BakedCellCentimeters = Recipe.CellCentimeters;
		if (!TestTrue(TEXT("版本配方编码"), FVoxelGenerationRecipeCodec::Encode(Recipe, Profile->RecipeBytes, Error))) return false;
		FVoxelGenerationRuntimeConfig Config;
		const bool bBound = FVoxelGenerationBinding::Build(*Profile, Registry, Recipe.Settings, Recipe.CellCentimeters, Config, Error);
		if (Version == 12)
		{
			TestFalse(TEXT("早于保留边界的配方拒绝绑定"), bBound);
			continue;
		}
		if (!TestTrue(TEXT("v13和v14分别允许真实运行绑定"), bBound)) return false;
		TestEqual(TEXT("绑定保留算法版本"), Config.Recipe->AlgorithmVersion, Version);
		Hashes.Add(Config.Recipe->RecipeHash);
	}
	TestTrue(TEXT("同种子不同自然算法具有独立身份"), Hashes.Num() == 2 && Hashes[0] != Hashes[1]);
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
	TestTrue(TEXT("Old recipe binds to its own cache"), First.BindHydrologyRecipe(100, 13, Error));
	First.Reset();
	TestTrue(TEXT("Same recipe may create fresh queries after content reset"), First.BindHydrologyRecipe(100, 13, Error));
	TestFalse(TEXT("Reset cannot change the original recipe identity"), First.BindHydrologyRecipe(200, 14, Error));
	TestTrue(TEXT("Recipe mismatch gives a lifecycle diagnosis"), Error.StartsWith(TEXT("InvalidRecipe")));
	FVoxelGenerationPlanCache Second;
	TestTrue(TEXT("New recipe binds to a new cache"), Second.BindHydrologyRecipe(200, 14, Error));
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
	TestTrue(TEXT("Original query generation binds"), Cache.BindHydrologyRecipe(100, 14, Error, Original));
	Cache.Reset();
	const uint64 Current = Cache.GetHydrologyGeneration();
	TestTrue(TEXT("Reset advances the generation"), Current > Original);
	TestFalse(TEXT("Stale query cannot bind or read a replacement generation"), Cache.BindHydrologyRecipe(100, 14, Error, Original));
	TestEqual(TEXT("Stale query is canceled"), Error, FString(TEXT("Canceled")));
	TestTrue(TEXT("Current same-recipe query can resume"), Cache.BindHydrologyRecipe(100, 14, Error, Current));
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
	TestTrue(TEXT("Rejected version does not poison the empty cache"), Cache.BindHydrologyRecipe(100, 13, Error));
	TestEqual(TEXT("Legacy dispatch boundary remains version 13"), FVoxelGenerationRecipe::LegacyHydrologyAlgorithmVersion, uint32(13));
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Voxel/Generation/Hydrology/VoxelHydrologyComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyReceivingSampleTest,
    "WHFramework.Voxel.Hydrology.Owner.ReceivingSample",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyReceivingSampleTest::RunTest(const FString& InParameters)
{
    (void)InParameters;
    FVoxelHydrologyRiverComponent Component;
    FVoxelStableId Body;
    Body.High = 1;
    Body.Low = 2;
    Component.ReceivingWaterByVoxel.Add(FIntPoint(1, 2), FVoxelHydrologyReceivingVoxel{0, 10, Body, false});
    FVoxelHydrologyInfluence Influence;
    TestTrue(TEXT("Existing receiving lake is sampled"), Component.Sample(1, 2, 0, Influence));
    TestEqual(TEXT("Existing lake keeps original bed"), Influence.GroundOverrideZ, 0);
    TestEqual(TEXT("Existing lake keeps fixed plane"), Influence.SurfaceWaterZ, 10);
    TestTrue(TEXT("Existing receiving lake remains a lake"), Influence.bLake && !Influence.bRiver);
    TestTrue(TEXT("Receiving voxel storage is charged"), Component.GetAllocatedBytes() > 0);
    Component.ExpandedReceivingWater = {-10, 8, Body, true};
    Component.LakeWaterByVoxel.Add(FIntPoint(-1, -2), 8);
    TestTrue(TEXT("Expanded ocean is sampled"), Component.Sample(-1, -2, -10, Influence));
    TestEqual(TEXT("Expanded ocean keeps original bed"), Influence.GroundOverrideZ, -10);
    TestTrue(TEXT("Expanded ocean retains its kind"), Influence.bOcean && !Influence.bLake);
    return true;
}

#endif
