#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformProcess.h"
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
	MapConfig.Recipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(MapRecipe));
	const auto Config = MakeShared<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>(MoveTemp(MapConfig));
	const auto Generator = MakeShared<const FVoxelGenerationPipeline, ESPMode::ThreadSafe>(
		Config, MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>());
	FVoxelTaskScheduler Scheduler;
	FVoxelMapTileCache Cache(Scheduler, Generator, Config, 71, 25.);
	const FVoxelMapTileKey Parent{FIntPoint(-1, -1), 512};
	const TArray<FVoxelMapTileKey> Requested{Parent};
	FSceneMapView View;
	Cache.SetColorResolver([](const FVoxelColumnSample&, const FVoxelGenerationRuntimeConfig&) { return FColor::Red; }, 1);
	Cache.Request(Requested, View);
	const FVoxelTaskDiagnostics Submitted = Scheduler.GetDiagnostics();
	TestTrue(TEXT("Old style has a submitted raster job before publication"), Submitted.Pending + Submitted.Running > 0);
	const FColor NewColor(19, 137, 73);
	Cache.SetColorResolver([NewColor](const FVoxelColumnSample&, const FVoxelGenerationRuntimeConfig&) { return NewColor; }, 2);
	const uint64 NewStyleRevision = Cache.GetRevision();
	auto Drain = [&]()
	{
		const double Deadline = FPlatformTime::Seconds() + 90.;
		while ((Scheduler.GetDiagnostics().Pending > 0 || Scheduler.ActiveCount() > 0) && FPlatformTime::Seconds() < Deadline)
		{
			Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.);
			FPlatformProcess::Sleep(0.001f);
		}
		return Scheduler.GetDiagnostics().Pending == 0 && Scheduler.ActiveCount() == 0;
	};
	const bool bOldFinished = Drain();
	TestTrue(TEXT("Old style job finishes"), bOldFinished);
	TestNull(TEXT("Old style cannot publish into the new cache"), Cache.FindBrush(Parent));
	TestEqual(TEXT("Discarded result does not increment publication revision"), Cache.GetRevision(), NewStyleRevision);
	Cache.Request(Requested, View);
	TestTrue(TEXT("New style job finishes"), Drain());
	const FSlateBrush* Brush = Cache.FindBrush(Parent);
	if (TestNotNull(TEXT("New style publishes a real parent tile"), Brush))
	{
		UTexture2D* Texture = Cast<UTexture2D>(Brush->GetResourceObject());
		if (TestNotNull(TEXT("Published tile has texture data"), Texture))
		{
			const FColor* Pixels = static_cast<const FColor*>(Texture->GetPlatformData()->Mips[0].BulkData.LockReadOnly());
			if (TestNotNull(TEXT("Published texture pixels are readable"), Pixels))
			{
				TestEqual(TEXT("The new resolver supplies the actual raster color"), Pixels[0], NewColor);
			}
			Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
		}
		const FVoxelMapTileKey Fine{FIntPoint(-1, -1), 4};
		FSlateBrush Fallback;
		TestNull(TEXT("Fine tile has not been generated"), Cache.FindBrush(Fine));
		if (TestTrue(TEXT("Zoom to an unbuilt fine tile keeps parent coverage"), Cache.FindDrawBrush(Fine, Fallback)))
		{
			TestEqual(TEXT("Fallback reuses the actual parent resource"), Fallback.GetResourceObject(), Brush->GetResourceObject());
			const FBox2f Region = Fallback.GetUVRegion();
			TestTrue(TEXT("Fallback samples only the requested negative child"),
				Region.Min.Equals(FVector2f(127.f / 128.f, 0.f)) && Region.Max.Equals(FVector2f(1.f, 1.f / 128.f)));
		}
	}
	const FVoxelTaskDiagnostics Diagnostics = Scheduler.GetDiagnostics();
	TestEqual(TEXT("The independent map pipeline emits only raster jobs"), Diagnostics.ByKind.Num(), 1);
	TestTrue(TEXT("Map sampling uses the map task kind"), Diagnostics.ByKind.Contains(EVoxelTaskKind::BuildMapTile));
	Scheduler.StopAndJoin();
	return true;
}

#endif
