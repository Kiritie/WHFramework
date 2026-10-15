#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Task/VoxelTerrainResources.h"

namespace VoxelTopologyRetentionTests
{
    void Maintain(FVoxelGenerationPlanCache& Cache)
    {
        for (int32 Index = 0; Index < FVoxelGenerationPlanCache::ShardCount * 2; ++Index)
        {
            Cache.TickMaintenance(256);
        }
    }

    bool Build(FVoxelGenerationPlanCache& Cache, const FVoxelHydrologyRegionKey& Key,
        FVoxelHydrologyPlanPtr& Plan, int32& Builds, FString& Error)
    {
        return Cache.GetOrBuildHydrologyTopology(Key, [&](FVoxelHydrologyPlan& Output, FString& BuildError)
        {
            ++Builds;
            Output.Key = Key;
            Output.Grid.WorldMinCell = Key.Coordinate * 8;
            Output.Grid.Width = 8;
            Output.Grid.Height = 8;
            Output.Grid.CellSize = 96;
            BuildError.Reset();
            return true;
        }, Plan, Error);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyTopologyTaskRetentionTest,
    "WHFramework.Voxel.Hydrology.Topology.TaskRetention",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyTopologyTaskRetentionTest::RunTest(const FString& Parameters)
{
    using namespace VoxelTopologyRetentionTests;
    (void)Parameters;
    const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
    const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
    if (!TestTrue(TEXT("Configure the original resource ledger"), Resources->Configure({}))) return false;
    const auto Consumer = FVoxelGenerationConsumerKey::Section(37, FGuid::NewGuid(), 1, {}, 1);
    FVoxelGenerationCacheRetentionPoint Point;
    Point.Center = FIntPoint(-1, -1);
    Point.HydrologyRadiusCells = 0;
    TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Retention;
    FString Error;
    if (!TestTrue(TEXT("Acquire the original task retention node"),
        Cache->RetainForTask(Point, Resources, Consumer, Retention, Error))) return false;

    const FVoxelHydrologyRegionKey Key{FIntPoint(-1, -1)};
    const FVoxelHydrologyRegionKey Outside{FIntPoint(-2, -1)};
    FVoxelHydrologyPlanPtr Plan;
    int32 Builds = 0;
    if (!TestTrue(TEXT("Build a topology touching the negative world-cell boundary"),
        Build(*Cache, Key, Plan, Builds, Error))) return false;
    Plan.Reset();
    Maintain(*Cache);
    TestTrue(TEXT("Task lease protects topology across a gap between readers"),
        Cache->FindHydrologyTopology(Key, Plan));
    Plan.Reset();
    TestTrue(TEXT("An adjacent query reuses the same input"), Build(*Cache, Key, Plan, Builds, Error));
    TestEqual(TEXT("Topology is constructed once while retained"), Builds, 1);
    Plan.Reset();

    TestTrue(TEXT("Build outside the actual physical retention bounds"),
        Build(*Cache, Outside, Plan, Builds, Error));
    Plan.Reset();
    Maintain(*Cache);
    TestFalse(TEXT("Unreferenced topology outside the task is retired"),
        Cache->FindHydrologyTopology(Outside, Plan));
    Retention.Reset();
    Maintain(*Cache);
    TestFalse(TEXT("The original topology becomes evictable after task release"),
        Cache->FindHydrologyTopology(Key, Plan));
    TestEqual(TEXT("Retention releases its original resource lease"), Resources->Snapshot().LeaseCount, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyTopologySourceRetentionTest,
    "WHFramework.Voxel.Hydrology.Topology.SourceRetentionAndExternalPin",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyTopologySourceRetentionTest::RunTest(const FString& Parameters)
{
    using namespace VoxelTopologyRetentionTests;
    (void)Parameters;
    const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
    const FVoxelHydrologyRegionKey Key{FIntPoint(1, 0)};
    FVoxelGenerationCacheRetention Retention;
    Retention.Revision = 1;
    FVoxelGenerationCacheRetentionPoint Point;
    Point.Center = FIntPoint(768, 0);
    Point.HydrologyRadiusCells = 0;
    Retention.Points.Add(Point);
    Cache->UpdateRetention(Retention);
    FVoxelHydrologyPlanPtr Plan;
    int32 Builds = 0;
    FString Error;
    if (!TestTrue(TEXT("Build topology at a source footprint edge"),
        Build(*Cache, Key, Plan, Builds, Error))) return false;
    Plan.Reset();
    Maintain(*Cache);
    if (!TestTrue(TEXT("Source intersects the real grid, independent of metadata center"),
        Cache->FindHydrologyTopology(Key, Plan))) return false;

    Retention.Points.Reset();
    ++Retention.Revision;
    Cache->UpdateRetention(Retention);
    Maintain(*Cache);
    FVoxelHydrologyPlanPtr Pinned;
    TestTrue(TEXT("Removing the source preserves an actual external reader"),
        Cache->FindHydrologyTopology(Key, Pinned));
    Pinned.Reset();
    Plan.Reset();
    Maintain(*Cache);
    TestFalse(TEXT("The final reader release allows normal retirement"),
        Cache->FindHydrologyTopology(Key, Plan));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyTopologyCapacityPressureTest,
    "WHFramework.Voxel.Hydrology.Topology.RetentionCapacityPressure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyTopologyCapacityPressureTest::RunTest(const FString& Parameters)
{
    using namespace VoxelTopologyRetentionTests;
    (void)Parameters;
    const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
    FVoxelTerrainResourceLimits Limits;
    Limits.ResidentBytes = 16ull * 1024ull * 1024ull;
    Limits.OverlapBytes = 0;
    if (!TestTrue(TEXT("Configure a finite original resident pool"), Resources->Configure(Limits))) return false;
    const auto Consumer = FVoxelGenerationConsumerKey::Section(37, FGuid::NewGuid(), 1, {}, 1);
    TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
    FString Error;
    if (!TestTrue(TEXT("Create the cache on its original ledger"),
        FVoxelGenerationPlanCache::Create(Resources, Consumer, Cache, Error, true, true))) return false;
    FVoxelGenerationCacheRetention Retention;
    Retention.Revision = 1;
    FVoxelGenerationCacheRetentionPoint Point;
    Point.HydrologyRadiusCells = 2048;
    Retention.Points.Add(Point);
    Cache->UpdateRetention(Retention);

    const FVoxelHydrologyRegionKey UnpinnedKey{FIntPoint(0, 0)};
    const FVoxelHydrologyRegionKey PinnedKey{FIntPoint(1, 0)};
    FVoxelHydrologyPlanPtr Unpinned;
    FVoxelHydrologyPlanPtr Pinned;
    int32 Builds = 0;
    if (!TestTrue(TEXT("Build an unpinned retained input"),
        Build(*Cache, UnpinnedKey, Unpinned, Builds, Error)) ||
        !TestTrue(TEXT("Build a separate active input"),
        Build(*Cache, PinnedKey, Pinned, Builds, Error))) return false;
    Unpinned.Reset();
    const auto State = Resources->Snapshot();
    const uint64 Used = State.Used[int32(EVoxelTerrainResourcePool::Resident)];
    if (!TestTrue(TEXT("Fixture has room for pressure without changing limits"),
        Used + 1 < State.Limits.ResidentBytes)) return false;
    TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Pressure;
    if (!TestTrue(TEXT("Fill remaining resident capacity through the actual ledger"),
        Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident,
            State.Limits.ResidentBytes - Used - 1, Pressure))) return false;
    Maintain(*Cache);
    FVoxelHydrologyPlanPtr Probe;
    TestFalse(TEXT("Retention remains optional when unreferenced backing must be reclaimed"),
        Cache->FindHydrologyTopology(UnpinnedKey, Probe));
    TestTrue(TEXT("Capacity pressure cannot retire an actual reader"),
        Cache->FindHydrologyTopology(PinnedKey, Probe));
    Probe.Reset();
    Pressure.Reset();
    Pinned.Reset();
    Retention.Points.Reset();
    ++Retention.Revision;
    Cache->UpdateRetention(Retention);
    Maintain(*Cache);
    TestFalse(TEXT("The last input retires after the real reader and source leave"),
        Cache->FindHydrologyTopology(PinnedKey, Probe));
    TestEqual(TEXT("No topology resident payload remains"),
        Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Resident)], uint64(0));
    return true;
}

#endif
