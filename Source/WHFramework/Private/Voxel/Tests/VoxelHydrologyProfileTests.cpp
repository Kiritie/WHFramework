#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyBasinClosure.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyProfile.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyProfileConfluenceTest,
    "WHFramework.Voxel.Hydrology.Profile.Confluence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelHydrologyProfileConfluenceTest::RunTest(const FString& InParameters)
{
    (void)InParameters;
    TArray<FVoxelRiverProfileNode> Nodes = {{0, 10, false, 0}, {0, 4, false, 0},
        {0, 8, false, 0}, {0, 7, true, 3}};
    TArray<FVoxelRiverProfileEdge> Edges = {{0, 2}, {1, 2}, {2, 3}};
    TArray<int32> Water;
    int32 Conflict = INDEX_NONE;
    FString Error;
    if (!TestTrue(TEXT("Shared finite river graph resolves"),
        VoxelHydrology::SolveRiverProfile(Nodes, Edges, Water, Conflict, Error))) return false;
    TestTrue(TEXT("Confluence is constrained by both upstream branches"), Water == TArray<int32>({10, 4, 4, 3}));
    Nodes[3].FixedWaterZ = 5;
    TestFalse(TEXT("Receiver higher than low tributary corridor is rejected"),
        VoxelHydrology::SolveRiverProfile(Nodes, Edges, Water, Conflict, Error));
    TestTrue(TEXT("Failed solve preserves previous completed output"), Water == TArray<int32>({10, 4, 4, 3}));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyProfileSharedPortTest,
    "WHFramework.Voxel.Hydrology.Profile.SharedPort",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelHydrologyProfileSharedPortTest::RunTest(const FString& InParameters)
{
    (void)InParameters;
    TArray<FVoxelRiverProfileNode> Full = {{0, 10, false, 0}, {0, 6, false, 0},
        {0, 9, false, 0}, {0, 8, true, 3}};
    TArray<FVoxelRiverProfileEdge> FullEdges = {{0, 1}, {1, 2}, {2, 3}};
    TArray<int32> Water;
    int32 Conflict = INDEX_NONE;
    FString Error;
    if (!TestTrue(TEXT("Owner resolves complete graph"),
        VoxelHydrology::SolveRiverProfile(Full, FullEdges, Water, Conflict, Error))) return false;
    TArray<FVoxelRiverProfileNode> Right = {{0, 6, true, Water[1]}, {0, 9, false, 0}, {0, 8, true, 3}};
    TArray<FVoxelRiverProfileEdge> RightEdges = {{0, 1}, {1, 2}};
    TArray<int32> RightWater;
    if (!TestTrue(TEXT("A solved shared port permits consistent subdivision"),
        VoxelHydrology::SolveRiverProfile(Right, RightEdges, RightWater, Conflict, Error))) return false;
    for (int32 Index = 0; Index < RightWater.Num(); ++Index)
        TestEqual(TEXT("Subdivision matches owner profile"), RightWater[Index], Water[Index + 1]);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyClosureResumeTest,
    "WHFramework.Voxel.Hydrology.Closure.Resume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelHydrologyClosureResumeTest::RunTest(const FString& InParameters)
{
    (void)InParameters;
    using ERead = EVoxelHydrologyClosureRead;
    using EStatus = EVoxelHydrologyClosureStatus;
    FVoxelHydrologyBasinClosure Closure;
    FString Error;
    bool bPending = true;
    auto Read = [&bPending](const FIntPoint& Cell, int32& Ground, FString&)
    {
        if (bPending && Cell == FIntPoint(-1, 0)) return ERead::Pending;
        Ground = FMath::Abs(Cell.X) <= 1 && FMath::Abs(Cell.Y) <= 1 ? 0 : 10;
        return ERead::Ready;
    };
    auto Connector = [](const FIntPoint&, const FInt64Vector2&, TOptional<int32>&, FString&)
    { return ERead::Ready; };
    if (!TestTrue(TEXT("Closure initialized"), Closure.Initialize(FIntPoint::ZeroValue, 5, Error))) return false;
    TestTrue(TEXT("Unknown terrain requests data without publishing"),
        Closure.Advance(Read, Connector, 100, 9).Status == EStatus::NeedTerrain && !Closure.IsReady());
    bPending = false;
    TestTrue(TEXT("Capacity block preserves the frontier"),
        Closure.Advance(Read, Connector, 100, 1).Status == EStatus::CapacityBlocked && !Closure.IsReady());
    FVoxelHydrologyClosureStep Step;
    int32 Calls = 0;
    do { Step = Closure.Advance(Read, Connector, 1, 9); ++Calls; }
    while (Step.Status == EStatus::Yielded && Calls < 40);
    if (!TestTrue(TEXT("Closure resumes to completion"), Step.Status == EStatus::Ready)) return false;
    TestEqual(TEXT("Exactly nine connected wet columns"), Closure.GetCells().Num(), 9);
    TestEqual(TEXT("Each shore face counted once after resume"), Closure.GetCertificate().ClosedEdges, uint32(12));
    Closure.FreeBacking();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyClosureConnectorTest,
    "WHFramework.Voxel.Hydrology.Closure.Connector",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelHydrologyClosureConnectorTest::RunTest(const FString& InParameters)
{
    (void)InParameters;
    using ERead = EVoxelHydrologyClosureRead;
    using EStatus = EVoxelHydrologyClosureStatus;
    auto Read = [](const FIntPoint& Cell, int32& Ground, FString&)
    { Ground = Cell == FIntPoint::ZeroValue ? 0 : 10; return ERead::Ready; };
    auto Connector = [](const FIntPoint&, const FInt64Vector2& To, TOptional<int32>& Plane, FString&)
    { if (To.X == -1 && To.Y == 0) Plane = 5; return ERead::Ready; };
    FString Error;
    FVoxelHydrologyBasinClosure Closure;
    if (!TestTrue(TEXT("Closure initialized"), Closure.Initialize(FIntPoint::ZeroValue, 5, Error))) return false;
    if (!TestTrue(TEXT("Certified same-plane ocean ends expansion"),
        Closure.Advance(Read, Connector, 100, 9).Status == EStatus::Ready)) return false;
    TestEqual(TEXT("One external receiving edge"), Closure.GetCertificate().ConnectorEdges, uint32(1));
    TestEqual(TEXT("Three actual dry shores"), Closure.GetCertificate().ClosedEdges, uint32(3));
    Closure.FreeBacking();
    return true;
}
#endif
