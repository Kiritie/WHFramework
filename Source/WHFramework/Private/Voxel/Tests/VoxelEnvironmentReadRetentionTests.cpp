#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationPlanningRead.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelEnvironmentReadRetentionTest,
	"WHFramework.Voxel.Generation.EnvironmentReadDependencyLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEnvironmentReadRetentionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(101, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	FString Error;
	if (!TestTrue(TEXT("Fixture uses its frozen configuration and actual original cache ledger"),
		FVoxelTerrainResources::Create({}, Consumer, Resources, Error) &&
		FVoxelGenerationBinding::Freeze(*VoxelTest::MakeGenerationConfig(), Resources.ToSharedRef(), Consumer, Config, Error) &&
		FVoxelGenerationPlanCache::Create(Resources.ToSharedRef(), Consumer, Cache, Error, true, false))) return false;
	FVoxelGenerationConsumerScope Identity(Consumer);
	FVoxelGenerationCacheRetention Retention;
	Retention.Revision = 1;
	Retention.HydrologyRegionSide = Config->Recipe->Settings.HydrologyRegionSide;
	Retention.HydrologyCellSize = Config->Recipe->Settings.HydrologyCellSize;
	if (!TestTrue(TEXT("World maintenance has no independent retention points"), Cache->UpdateRetention(Retention, 0,
		[](TArrayView<FVoxelGenerationCacheRetentionPoint>, FString&) { return true; }, Error))) return false;
	const int32 Side = Config->Recipe->Settings.HydrologyRegionSide * Config->Recipe->Settings.HydrologyCellSize;
	{
		TAtomic<bool> Cancel(false);
		FVoxelGenerationPlanningReadScope ColdScope(nullptr, nullptr, &Cancel, &Consumer, false);
		TestFalse(TEXT("Cancellation propagation alone keeps cold planning permission"),
			FVoxelGenerationPlanningReadScope::RequiresReadyPlans());
		FVoxelGenerationQuery Cold;
		FVoxelColumnSample Column;
		if (!TestTrue(TEXT("Query created inside cancellation scope builds its actual missing hydrology"),
			FVoxelGenerationQuery::Create(Config.ToSharedRef(), Cache.ToSharedRef(), Cold, Error, false, false, false, &Consumer) &&
			Cold.PrepareEnvironmentColumnRead(Side * 8, 0, Error) &&
			Cold.SampleEnvironmentColumn(Side * 8, 0, Column, Error))) return false;
		{
			FVoxelGenerationPlanningReadScope Worker;
			FVoxelGenerationPlanningReadScope NestedCancel(nullptr, nullptr, &Cancel, &Consumer, false);
			TestTrue(TEXT("Nested cancellation scope cannot weaken the original Worker preflight permission"),
				FVoxelGenerationPlanningReadScope::RequiresReadyPlans());
			FVoxelGenerationQuery Ready;
			if (!TestTrue(TEXT("Nested query inherits the original Worker ready-only contract"),
				FVoxelGenerationQuery::Create(Config.ToSharedRef(), Cache.ToSharedRef(), Ready, Error, false, false, false, &Consumer))) return false;
			TestFalse(TEXT("Inherited ready-only query does not build an absent dependency"),
				Ready.SampleEnvironmentColumn(Side * 9, 0, Column, Error));
			TestTrue(TEXT("Inherited missing dependency remains explicit"), Error.StartsWith(TEXT("DependencyNotReady")));
		}
		Cancel.Store(true);
		TestFalse(TEXT("Cold cancellation scope still rejects its original warm reader"), Cold.PrepareEnvironmentColumnRead(Side * 8, 0, Error));
		TestEqual(TEXT("Cold scope propagates the actual cancellation"), Error, FString(TEXT("Canceled")));
	}
	for (int32 Shard = 0; Shard < FVoxelGenerationPlanCache::ShardCount; ++Shard) Cache->TickMaintenance(4096);
	const FIntPoint Points[] = {{0, 0}, {-Side, 0}, {Side, 0}, {0, Side}, {0, 0}, {-Side, 0}};
	TArray<FVoxelHydrologyRegionKey, TFixedAllocator<4>> Keys;
	TArray<FVoxelHydrologyPlanPtr, TFixedAllocator<4>> Bodies;
	{
		FVoxelGenerationQuery Query;
		if (!TestTrue(TEXT("Cold operation creates one original query without global column caching"),
			FVoxelGenerationQuery::Create(Config.ToSharedRef(), Cache.ToSharedRef(), Query, Error, false, false, false, &Consumer))) return false;
		for (const auto& Point : Points)
		{
			FVoxelColumnSample Column;
			if (!TestTrue(TEXT("Actual cold environment read retains its original hydrology dependency"),
				Query.PrepareEnvironmentColumnRead(Point.X, Point.Y, Error) &&
				Query.SampleEnvironmentColumn(Point.X, Point.Y, Column, Error)))
			{
				AddError(Error);
				return false;
			}
			const auto Key = FVoxelGenerationQuery::HydrologyKeyForVoxel(Point.X, Point.Y, Config->Recipe->Settings);
			if (!Keys.Contains(Key))
			{
				FVoxelHydrologyPlanPtr Body;
				if (!TestTrue(TEXT("Actual sampled body is present in the original cache"), Cache->FindHydrology(Key, Body))) return false;
				Keys.Add(Key);
				Bodies.Add(MoveTemp(Body));
			}
			for (int32 Shard = 0; Shard < FVoxelGenerationPlanCache::ShardCount; ++Shard) Cache->TickMaintenance(4096);
			for (int32 Index = 0; Index < Keys.Num(); ++Index)
			{
				FVoxelHydrologyPlanPtr Body;
				TestTrue(TEXT("Earlier actual dependencies keep the same immutable body after read range growth and maintenance"),
					Cache->FindHydrology(Keys[Index], Body) && Body == Bodies[Index]);
			}
		}
		TestEqual(TEXT("Operation samples four actual hydrology regions"), Keys.Num(), 4);
		TestEqual(TEXT("Single operation reads do not populate the global natural-column cache"), Cache->GetStats().NaturalColumns, 0);
		TAtomic<bool> Cancel(true);
		const uint64 BeforeCancel = Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)];
		{
			FVoxelGenerationPlanningReadScope Read(nullptr, nullptr, &Cancel, &Consumer);
			TestFalse(TEXT("Original cancellation rejects a warm environment read before changing its retention"),
				Query.PrepareEnvironmentColumnRead(0, 0, Error));
			TestEqual(TEXT("Original cancellation remains explicit"), Error, FString(TEXT("Canceled")));
		}
		TestEqual(TEXT("Rejected read acquires no new retention grant"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)], BeforeCancel);
		FVoxelGenerationQuery Ready;
		if (!TestTrue(TEXT("Ready-only query keeps the original preflight contract"),
			FVoxelGenerationQuery::Create(Config.ToSharedRef(), Cache.ToSharedRef(), Ready, Error, false, true, false, &Consumer))) return false;
		const uint64 BeforeReady = Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)];
		FVoxelColumnSample Missing;
		TestTrue(TEXT("Preparing a ready-only read does not create a cold dependency"), Ready.PrepareEnvironmentColumnRead(Side * 4, 0, Error));
		TestFalse(TEXT("Absent ready-only hydrology remains missing"), Ready.SampleEnvironmentColumn(Side * 4, 0, Missing, Error));
		TestTrue(TEXT("Missing original dependency is explicit and did not acquire retention"),
			Error.StartsWith(TEXT("DependencyNotReady")) && Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)] == BeforeReady);
	}
	for (int32 Shard = 0; Shard < FVoxelGenerationPlanCache::ShardCount; ++Shard) Cache->TickMaintenance(4096);
	TestEqual(TEXT("Completing the cold operation permits actual eviction of all sampled dependencies"), Cache->GetStats().Hydrology, 0);
	TestEqual(TEXT("Completed operation releases all original Working backing"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	return true;
}

#endif
