#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeExit.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "Voxel/Save/VoxelRegionStore.h"
#include "Voxel/Streaming/VoxelEmergeManager.h"
#include "Voxel/Map/VoxelMapTileCache.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMapNegativeParentCoverageTest,
	"WHFramework.Voxel.Map.NegativeParentAndTwentyKilometerCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelMapNegativeParentCoverageTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const FVoxelMapTileKey Child{FIntPoint(-1, -33), 16};
	const FVoxelMapTileKey Parent = FVoxelMapTileCache::ParentKey(Child, 512);
	TestEqual(TEXT("Negative X floors into the preceding parent"), Parent.Coordinate.X, -1);
	TestEqual(TEXT("Negative Y crosses the preceding parent boundary"), Parent.Coordinate.Y, -2);
	const FBox2f UV = FVoxelMapTileCache::ParentUV(Child, Parent);
	TestTrue(TEXT("Negative child uses the bottom-right 1/32 area after raster Y inversion"),
		UV.Min.Equals(FVector2f(31.f / 32.f, 0.f)) && UV.Max.Equals(FVector2f(1.f, 1.f / 32.f)));

	const FVector2D PanelSize(1000., 700.);
	TSet<int32> Steps;
	for (const float Range : {20000.f, 50000.f, 100000.f, 200000.f, 500000.f, 2000000.f})
	{
		for (const float Yaw : {0.f, 37.f, 90.f})
		{
			FSceneMapView View;
			View.Center = FVector2D(-812537., -402019.);
			View.Range = Range;
			View.Yaw = Yaw;
			TArray<FVoxelMapTileKey> Keys;
			FVoxelMapTileCache::VisibleTiles(View, PanelSize, 25., Keys);
			if (!TestTrue(TEXT("Every range has visible tiles"), !Keys.IsEmpty())) return false;
			Steps.Add(Keys[0].Step);
			TSet<FVoxelMapTileKey> Unique;
			for (const FVoxelMapTileKey& Key : Keys) Unique.Add(Key);
			TestEqual(TEXT("Coverage contains no duplicate keys"), Unique.Num(), Keys.Num());
			for (int32 Row = 0; Row <= 10; ++Row)
			{
				for (int32 Column = 0; Column <= 10; ++Column)
				{
					const FVector2D Panel(Column * PanelSize.X / 10., Row * PanelSize.Y / 10.);
					const FVector2D World = View.PanelToWorld(Panel, PanelSize);
					TestTrue(TEXT("Map world/panel round trip preserves the same view"), View.WorldToPanel(World, PanelSize).Equals(Panel, 0.001));
					const double TileWidth = 25. * FVoxelMapTileCache::TileSide * Keys[0].Step;
					const FVoxelMapTileKey Required{FIntPoint(FMath::FloorToInt(World.X / TileWidth), FMath::FloorToInt(World.Y / TileWidth)), Keys[0].Step};
					TestTrue(TEXT("Every sampled panel point has a tile through pan, rotation and 20 km zoom"), Unique.Contains(Required));
				}
			}
		}
	}
	TestEqual(TEXT("The coverage matrix exercises all six raster steps"), Steps.Num(), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMapStyleEpochTest,
	"WHFramework.Voxel.Map.StaleStyleResultAndCoarseFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelMapStyleEpochTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationRuntimeConfig MapConfig = *VoxelTest::MakeGenerationConfig();
	FVoxelGenerationRecipe MapRecipe = *MapConfig.Recipe;
	MapRecipe.Settings.HydrologyRegionSide = 16;
	MapRecipe.Settings.HydrologyHaloCells = 4;
	MapRecipe.Settings.RiverSourceAccumulation = MAX_int32;
	MapConfig.Recipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(MapRecipe));
	const auto Config = MakeShared<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>(MoveTemp(MapConfig));
	const auto Plans = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelGenerationCacheRetention CacheRetention;
	CacheRetention.HydrologyRegionSide = Config->Recipe->Settings.HydrologyRegionSide;
	CacheRetention.HydrologyCellSize = Config->Recipe->Settings.HydrologyCellSize;
	Plans->UpdateRetention(CacheRetention);
	const auto Generator = MakeShared<const FVoxelGenerationPipeline, ESPMode::ThreadSafe>(Config, Plans, nullptr, true);
	const auto Registry = VoxelTest::MakeRegistry();
	FVoxelTaskScheduler Scheduler(true);
	FVoxelWorldRuntime Runtime(71, true, Registry, Generator, Scheduler.GetRuntimeResources());
	FVoxelRegionStore RegionStore;
	FVoxelWorldManifest Manifest;
	FVoxelEmergeManager Emerge(Runtime, Scheduler, Generator, Config, Plans, RegionStore, Manifest, Registry);
	FVoxelMapTileCache Cache(Scheduler, Emerge, Generator, Config, 71, 25.);
	ON_SCOPE_EXIT
	{
		Scheduler.StopAndJoin();
	};
	Emerge.SetExternalConsumerWakeCallback([&Cache](const FVoxelGenerationConsumerKey& InConsumer)
	{
		Cache.OnGenerationDependencyReady(InConsumer);
	});
	const FVoxelInterestSet EmptyInterest;
	uint64 PeakRetentionBytes = 0;
	uint64 ObservedRuntimeInputBytes = 0;
	uint64 ObservedRuntimeResidentBytes = 0;
	uint64 ObservedTerrainStagedBytes = 0;
	int32 PeakRetentionCount = 0;
	FVoxelGenerationConsumerKey PreviousBatchConsumer;
	int32 PreviousBatchRow = INDEX_NONE;
	TWeakPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> PreviousBatchRetention;
	bool bObservedRowRelease = false;
	const auto Advance = [&]()
	{
		Emerge.Tick(EmptyInterest, 0, FPlatformTime::Seconds(), 1, 8.);
		Cache.Tick(FPlatformTime::Seconds() + 0.008);
		for (const auto& Pair : Cache.Pending)
		{
			const auto* Capture = Emerge.ExternalFactConsumers.Find(Pair.Value.Consumer);
			if (!Capture) continue;
			TestTrue(TEXT("稀疏跨行生成始终只保留本批最多64个原计划租约"), Capture->SampleRetentions.Num() <= 64);
			PeakRetentionCount = FMath::Max(PeakRetentionCount, Capture->SampleRetentions.Num());
			uint64 Bytes = Capture->Dependencies.Lease ? Capture->Dependencies.Lease->Bytes() : 0;
			Bytes += Capture->SampleRetentionStorageLease ? Capture->SampleRetentionStorageLease->Bytes() : 0;
			for (const auto& Retention : Capture->SampleRetentions) Bytes += Retention->GetResourceLease()->Bytes();
			PeakRetentionBytes = FMath::Max(PeakRetentionBytes, Bytes);
			if (Capture->SampleRetentions.Num() == 64)
			{
				if (PreviousBatchConsumer == Pair.Value.Consumer && Pair.Value.PreflightStartRow > PreviousBatchRow)
				{
					TestFalse(TEXT("进入下一稀疏行前原行retention已销毁"), PreviousBatchRetention.IsValid());
					bObservedRowRelease = true;
				}
				PreviousBatchConsumer = Pair.Value.Consumer;
				PreviousBatchRow = Pair.Value.PreflightStartRow;
				PreviousBatchRetention = Capture->SampleRetentions[0];
			}
		}
		Plans->TickMaintenance(256);
		Scheduler.Tick([&Emerge](FVoxelTaskResult&& InResult) { Emerge.OnTask(MoveTemp(InResult)); }, 8.);
		const auto RuntimeResources = Scheduler.GetRuntimeResources()->Snapshot();
		const auto TerrainResources = Scheduler.GetTerrainResources()->Snapshot();
		ObservedRuntimeInputBytes = FMath::Max(ObservedRuntimeInputBytes,
			RuntimeResources.Used[int32(EVoxelTerrainResourcePool::Input)]);
		ObservedRuntimeResidentBytes = FMath::Max(ObservedRuntimeResidentBytes,
			RuntimeResources.Used[int32(EVoxelTerrainResourcePool::Resident)]);
		ObservedTerrainStagedBytes = FMath::Max(ObservedTerrainStagedBytes,
			TerrainResources.Used[int32(EVoxelTerrainResourcePool::Staged)]);
	};
	const auto Drain = [&]()
	{
		const double Deadline = FPlatformTime::Seconds() + 90.;
		while (FPlatformTime::Seconds() < Deadline)
		{
			Advance();
			for (const auto& Pair : Cache.Pending)
			{
				if (Pair.Value.bFailed) return false;
			}
			const auto Diagnostics = Scheduler.GetDiagnostics();
			if (Cache.Pending.IsEmpty() && Diagnostics.Pending == 0 && Scheduler.ActiveCount() == 0 &&
				Diagnostics.NativeInFlight == 0 && Diagnostics.CompletedAwaitingApply == 0) return true;
			FPlatformProcess::Sleep(0.001f);
		}
		return false;
	};
	const FVoxelMapTileKey Parent{FIntPoint(-1, -1), 512};
	const TArray<FVoxelMapTileKey> Requested{Parent};
	FSceneMapView View;
	Cache.SetColorResolver([](const FVoxelColumnSample&, const FVoxelGenerationRuntimeConfig&) { return FColor::Red; }, 1);
	Cache.Request(Requested, View);
	if (!TestTrue(TEXT("地图只建立一个原始请求"), Cache.Pending.Contains(Parent))) return false;
	const auto OriginalConsumer = Cache.Pending.FindChecked(Parent).Consumer;
	for (int32 Index = 0; Index < 32; ++Index)
	{
		Cache.Request(Requested, View);
		Cache.Tick(FPlatformTime::Seconds() + 0.008);
	}
	TestTrue(TEXT("真实计划结果尚未应用时重复请求不更换Consumer"),
		Cache.Pending.FindChecked(Parent).Consumer == OriginalConsumer);
	TestNull(TEXT("依赖未就绪不发布占位纹理"), Cache.FindBrush(Parent));
	const double AdmissionDeadline = FPlatformTime::Seconds() + 90.;
	while (FPlatformTime::Seconds() < AdmissionDeadline)
	{
		const auto* Current = Cache.Pending.Find(Parent);
		if (!Current || Current->bFailed || Current->bSubmitted) break;
		Advance();
		FPlatformProcess::Sleep(0.001f);
	}
	const auto* OldState = Cache.Pending.Find(Parent);
	if (!TestTrue(TEXT("旧样式经过真实依赖预检后接纳raster任务"), OldState && OldState->bSubmitted)) return false;
	const FColor NewColor(19, 137, 73);
	Cache.SetColorResolver([NewColor](const FVoxelColumnSample&, const FVoxelGenerationRuntimeConfig&) { return NewColor; }, 2);
	const uint64 NewStyleRevision = Cache.GetRevision();
	TestTrue(TEXT("旧样式任务取消并完成回收"), Drain());
	TestNull(TEXT("旧样式不能发布到新缓存"), Cache.FindBrush(Parent));
	TestEqual(TEXT("丢弃结果不增长发布Revision"), Cache.GetRevision(), NewStyleRevision);
	Cache.Request(Requested, View);
	TestTrue(TEXT("停止重复UI Request后已接纳瓦片仍由真实Emerge和Map Tick完成"), Drain());
	TestTrue(TEXT("瓦片完成后归还Map请求目录租约"), !Cache.PendingStorageLease && Cache.PendingCapacity == 0);
	TestTrue(TEXT("稀疏跨行实际经历前批保留释放"), bObservedRowRelease);
	TestTrue(TEXT("完成后原外部事实消费者已注销"), Emerge.ExternalFactConsumers.IsEmpty());
	AddInfo(FString::Printf(TEXT("Sparse map sampled-dependency capture and retention peak: leases=%d bytes=%llu"),
		PeakRetentionCount, PeakRetentionBytes));
	AddInfo(FString::Printf(TEXT("Sparse map GT-observed fixture bytes: runtimeInput=%llu runtimeResident=%llu terrainStaged=%llu"),
		ObservedRuntimeInputBytes, ObservedRuntimeResidentBytes, ObservedTerrainStagedBytes));
	const FSlateBrush* Brush = Cache.FindBrush(Parent);
	if (TestNotNull(TEXT("新样式发布真实父瓦片"), Brush))
	{
		UTexture2D* Texture = Cast<UTexture2D>(Brush->GetResourceObject());
		if (TestNotNull(TEXT("发布瓦片含真实纹理数据"), Texture))
		{
			const FColor* Pixels = static_cast<const FColor*>(Texture->GetPlatformData()->Mips[0].BulkData.LockReadOnly());
			if (TestNotNull(TEXT("发布纹理像素可读"), Pixels))
			{
				for (int32 Index = 0; Index < FVoxelMapTileCache::TileSide * FVoxelMapTileCache::TileSide; ++Index)
				{
					if (Pixels[Index] != NewColor)
					{
						AddError(FString::Printf(TEXT("新Resolver像素不一致: %d"), Index));
						break;
					}
				}
			}
			Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
		}
		const FVoxelMapTileKey Fine{FIntPoint(-1, -1), 4};
		FSlateBrush Fallback;
		TestNull(TEXT("细瓦片尚未生成"), Cache.FindBrush(Fine));
		if (TestTrue(TEXT("缩放到未完成细瓦片仍保留原父覆盖"), Cache.FindDrawBrush(Fine, Fallback)))
		{
			TestEqual(TEXT("Fallback沿用实际父资源"), Fallback.GetResourceObject(), Brush->GetResourceObject());
			const FBox2f Region = Fallback.GetUVRegion();
			TestTrue(TEXT("Fallback只采样原负坐标子区域"),
				Region.Min.Equals(FVector2f(127.f / 128.f, 0.f)) && Region.Max.Equals(FVector2f(1.f, 1.f / 128.f)));
		}
	}
	const FVoxelMapTileKey CanceledKey{FIntPoint(2, 2), 512};
	Cache.Request({CanceledKey}, View);
	Cache.Tick(FPlatformTime::Seconds() + 0.008);
	TestTrue(TEXT("可注销的冷请求已登记"), Cache.Pending.Contains(CanceledKey));
	Cache.Request({}, View);
	TestTrue(TEXT("视图移出后原请求被移除"), Cache.Pending.IsEmpty());
	TestTrue(TEXT("注销后真实计划任务最终退出"), Drain());
	TestNull(TEXT("注销后的瓦片结果不能发布"), Cache.FindBrush(CanceledKey));
	const FVoxelTaskDiagnostics Diagnostics = Scheduler.GetDiagnostics();
	TestTrue(TEXT("地图通过原计划任务产生真实Hydrology依赖"), Diagnostics.ByKind.Contains(EVoxelTaskKind::BuildGenerationPlan));
	TestTrue(TEXT("地图采样保留BuildMapTile任务种类"), Diagnostics.ByKind.Contains(EVoxelTaskKind::BuildMapTile));
	if (const auto* Raster = Diagnostics.ByKind.Find(EVoxelTaskKind::BuildMapTile))
	{
		TestEqual(TEXT("依赖等待不转换成raster失败重建"), Raster->Failed, uint64(0));
		TestEqual(TEXT("多批次只有旧样式取消和新样式完成两个终态"), Raster->Completed, uint64(2));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMapSparseDependenciesTest,
	"WHFramework.Voxel.Map.SparseSampleDependencies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelMapSparseDependenciesTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationRuntimeConfig MutableConfig = *VoxelTest::MakeGenerationConfig();
	FVoxelGenerationRecipe Recipe = *MutableConfig.Recipe;
	Recipe.Settings.HydrologyRegionSide = 16;
	MutableConfig.Recipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe));
	const auto Config = MakeShared<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>(MoveTemp(MutableConfig));
	const auto Plans = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelTaskScheduler Scheduler(true);
	FVoxelGenerationPlanCoordinator Coordinator(Scheduler, Config, Plans, 73);
	const FGuid Owner = FGuid::NewGuid();
	const FIntPoint Origin(-32768, -32768);
	for (const int32 Step : {4, 128, 512})
	{
		const FIntVector Max(Origin.X + 63 * Step + 1, Origin.Y + 63 * Step + 1, Config->Recipe->Settings.MaxZ);
		const FVoxelGenerationConsumerKey Consumer{EVoxelGenerationConsumerKind::Environment, 73, Owner, 1,
			EVoxelGenerationSpatialKind::Bounds,
			FIntVector(Origin.X, Origin.Y, Config->Recipe->Settings.MinZ), Max, uint64(Step)};
		FVoxelGenerationPlanDependencyCapture Capture;
		FString Error;
		if (!TestTrue(TEXT("实际64乘64采样点可捕获Hydrology依赖"),
			Coordinator.GatherEnvironmentSamples(Origin, 64, 64, Step, Consumer, Capture, Error)))
		{
			AddError(Error);
			return false;
		}
		TSet<FIntVector> Expected;
		for (int32 Row = 0; Row < 64; ++Row)
		{
			for (int32 Column = 0; Column < 64; ++Column)
			{
				const auto Key = FVoxelGenerationQuery::HydrologyKeyForVoxel(
					Origin.X + Column * Step, Origin.Y + Row * Step, Config->Recipe->Settings);
				Expected.Add(FIntVector(Key.Coordinate.X, Key.Coordinate.Y, 0));
			}
		}
		TestEqual(TEXT("依赖数等于真实采样点去重数"), Capture.Values.Num(), Expected.Num());
		TSet<FIntVector> Actual;
		for (const auto& Dependency : Capture.Values)
		{
			TestTrue(TEXT("地图不附加洞穴、生态或结构事实"), Dependency.Kind == EVoxelGenerationPlanKind::Hydrology);
			TestTrue(TEXT("每个依赖都来自真实采样点"), Expected.Contains(Dependency.Coordinate()));
			Actual.Add(Dependency.Coordinate());
		}
		TestEqual(TEXT("依赖没有重复"), Actual.Num(), Capture.Values.Num());
		if (Step == 512)
		{
			TestEqual(TEXT("稀疏大步长保留4096个实采依赖"), Capture.Values.Num(), 4096);
			const auto First = FVoxelGenerationQuery::HydrologyKeyForVoxel(Origin.X, Origin.Y, Config->Recipe->Settings);
			const auto Last = FVoxelGenerationQuery::HydrologyKeyForVoxel(Max.X - 1, Max.Y - 1, Config->Recipe->Settings);
			const int64 DenseCount = int64(Last.Coordinate.X - First.Coordinate.X + 1) *
				(Last.Coordinate.Y - First.Coordinate.Y + 1);
			TestTrue(TEXT("同包围矩形密集依赖超过4096也不影响稀疏请求"), DenseCount > 4096);
		}
	}
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMapCacheLifetimeTest,
	"WHFramework.Voxel.Map.CacheDestructionBeforeSchedulerDrain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelMapCacheLifetimeTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationRuntimeConfig MutableConfig = *VoxelTest::MakeGenerationConfig();
	FVoxelGenerationRecipe Recipe = *MutableConfig.Recipe;
	Recipe.Settings.HydrologyRegionSide = 16;
	Recipe.Settings.HydrologyHaloCells = 4;
	Recipe.Settings.RiverSourceAccumulation = MAX_int32;
	MutableConfig.Recipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe));
	const auto Config = MakeShared<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>(MoveTemp(MutableConfig));
	const auto Plans = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelGenerationCacheRetention CacheRetention;
	CacheRetention.HydrologyRegionSide = Config->Recipe->Settings.HydrologyRegionSide;
	CacheRetention.HydrologyCellSize = Config->Recipe->Settings.HydrologyCellSize;
	Plans->UpdateRetention(CacheRetention);
	const auto Generator = MakeShared<const FVoxelGenerationPipeline, ESPMode::ThreadSafe>(Config, Plans, nullptr, true);
	const auto Registry = VoxelTest::MakeRegistry();
	FVoxelTaskScheduler Scheduler(true);
	FVoxelWorldRuntime Runtime(79, true, Registry, Generator, Scheduler.GetRuntimeResources());
	FVoxelRegionStore RegionStore;
	FVoxelWorldManifest Manifest;
	TUniquePtr<FVoxelMapTileCache> Cache;
	FVoxelEmergeManager Emerge(Runtime, Scheduler, Generator, Config, Plans, RegionStore, Manifest, Registry);
	Cache = MakeUnique<FVoxelMapTileCache>(Scheduler, Emerge, Generator, Config, 79, 25.);
	ON_SCOPE_EXIT
	{
		Scheduler.StopAndJoin();
		Cache.Reset();
	};
	Emerge.SetExternalConsumerWakeCallback([&Cache](const FVoxelGenerationConsumerKey& InConsumer)
	{
		if (Cache) Cache->OnGenerationDependencyReady(InConsumer);
	});
	Cache->SetColorResolver([](const FVoxelColumnSample&, const FVoxelGenerationRuntimeConfig&) { return FColor::Blue; }, 1);
	const FVoxelMapTileKey Key{FIntPoint(-1, -1), 512};
	Cache->Request({Key}, FSceneMapView{});
	const FVoxelInterestSet EmptyInterest;
	TWeakPtr<FVoxelMapTileCache::FBuildWork, ESPMode::ThreadSafe> RetiredWork;
	bool bRasterAdmitted = false;
	const double AdmissionDeadline = FPlatformTime::Seconds() + 90.;
	while (FPlatformTime::Seconds() < AdmissionDeadline)
	{
		Emerge.Tick(EmptyInterest, 0, FPlatformTime::Seconds(), 1, 8.);
		Cache->Tick(FPlatformTime::Seconds() + 0.008);
		const auto* Current = Cache->Pending.Find(Key);
		if (!Current || Current->bFailed) break;
		if (Current->bSubmitted)
		{
			RetiredWork = Current->Work;
			bRasterAdmitted = true;
			break;
		}
		Scheduler.Tick([&Emerge](FVoxelTaskResult&& InResult) { Emerge.OnTask(MoveTemp(InResult)); }, 8.);
		FPlatformProcess::Sleep(0.001f);
	}
	TestTrue(TEXT("析构前原raster Apply已绑定并进入真实调度器"), bRasterAdmitted);
	Cache.Reset();
	bool bDrained = false;
	const double DrainDeadline = FPlatformTime::Seconds() + 90.;
	while (FPlatformTime::Seconds() < DrainDeadline)
	{
		Emerge.Tick(EmptyInterest, 0, FPlatformTime::Seconds(), 1, 8.);
		Scheduler.Tick([&Emerge](FVoxelTaskResult&& InResult) { Emerge.OnTask(MoveTemp(InResult)); }, 8.);
		const auto Diagnostics = Scheduler.GetDiagnostics();
		if (Diagnostics.Pending == 0 && Scheduler.ActiveCount() == 0 && Diagnostics.NativeInFlight == 0 &&
			Diagnostics.CompletedAwaitingApply == 0 && !RetiredWork.IsValid())
		{
			bDrained = true;
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	TestTrue(TEXT("Map已析构后原调度器继续排出取消回执"), bDrained);
	TestFalse(TEXT("旧Apply失效且原Work最终释放"), RetiredWork.IsValid());
	return true;
}

#endif
