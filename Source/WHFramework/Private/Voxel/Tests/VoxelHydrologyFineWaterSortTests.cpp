#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyFineWaterQuery.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

namespace
{
    struct FFineWaterSortFixture
    {
        int32 Width;
        int32 Height;
        FIntPoint Minimum() const { return FIntPoint(-7, -11); }
        FIntPoint Maximum() const { return Minimum() + FIntPoint(Width, Height - 1); }
        FIntPoint Seed() const { return Minimum() + FIntPoint(Width / 2, Height / 2); }
        FIntPoint Receiver() const { return Minimum() + FIntPoint(Width, 0); }
        int32 Ground(const FIntPoint& Cell) const
        {
            const FIntPoint Local = Cell - Minimum();
            return -1 - ((Local.X * 7 + Local.Y * 3) % 17);
        }
        EVoxelHydrologyClosureRead Read(const FIntPoint& Position,
            FVoxelHydrologySharedCell& Out, FString& Error) const
        {
            Error.Reset();
            Out = FVoxelHydrologySharedCell();
            Out.Cell = Position;
            Out.GroundZ = Ground(Position);
            if (Position == Receiver())
            {
                Out.GroundZ = -20;
                Out.ReceiverWaterZ = 10;
                Out.ReceiverId.High = 17;
                Out.ReceiverId.Low = 29;
            }
            else if (Position.X == Maximum().X)
            {
                Out.GroundZ = 20;
            }
            return EVoxelHydrologyClosureRead::Ready;
        }
    };

    // 排序阶段分别计入元素输出、归并段结束、数组交换与进入证书发布的工作量。
    int64 ExpectedSortSteps(int32 Count)
    {
        int64 Steps = 1;
        for (int64 Width = 1; Width < Count; Width *= 2)
        {
            Steps += Count + (Count + 2 * Width - 1) / (2 * Width) + 1;
        }
        return Steps;
    }

    bool CheckReady(FAutomationTestBase& Test, const FFineWaterSortFixture& Fixture,
        const FVoxelHydrologyFineWaterQuery& Query)
    {
        if (!Test.TestTrue(TEXT("Query is ready"), Query.IsReady())) return false;
        const auto& Cells = Query.GetCells();
        if (!Test.TestEqual(TEXT("Complete natural member set retained"),
            Cells.Num(), Fixture.Width * Fixture.Height)) return false;
        Test.TestTrue(TEXT("Natural-water identity retained"), Query.GetResult().bNaturalWater);
        Test.TestFalse(TEXT("Natural body did not become fixed water"), Query.GetResult().bFixedWater);
        Test.TestEqual(TEXT("Exact water plane retained"), Query.GetResult().WaterZ, 10);
        Test.TestEqual(TEXT("Natural key water retained"), Query.GetResult().NaturalKey.WaterZ, 10);
        Test.TestEqual(TEXT("Natural key minimum retained"),
            Query.GetResult().NaturalKey.MinimumMember, Fixture.Minimum());
        Test.TestEqual(TEXT("Seed ground retained"), Query.GetSeedGround(), Fixture.Ground(Fixture.Seed()));
        int32 Index = 0;
        for (int32 X = 0; X < Fixture.Width; ++X)
        {
            for (int32 Y = 0; Y < Fixture.Height; ++Y, ++Index)
            {
                const FIntPoint Expected = Fixture.Minimum() + FIntPoint(X, Y);
                Test.TestEqual(TEXT("Lexicographic cell order retained"), Cells[Index], Expected);
                Test.TestEqual(TEXT("Ground stays paired with its cell"),
                    Query.GetMemberGround(Index), Fixture.Ground(Expected));
            }
        }
        Test.TestTrue(TEXT("Physical wet contact retained"),
            Query.GetFixedContacts().ContainsByPredicate([&Fixture](const FVoxelHydrologySharedCell& Cell)
            { return Cell.Cell == Fixture.Receiver() && Cell.ReceiverWaterZ == 10; }));
        Test.TestTrue(TEXT("Wet receiver certificate retained"),
            Query.GetDrainageCertificate().ContainsByPredicate([&Fixture](const FVoxelHydrologyFineWaterCertificate& Cell)
            { return Cell.Cell == Fixture.Receiver() && Cell.IsReceiver(); }));
        return true;
    }

    bool RunFixture(FAutomationTestBase& Test, FVoxelHydrologyFineWaterQuery& Query,
        const FFineWaterSortFixture& Fixture, int32 Budget, bool bInterruptSort)
    {
        FString Error;
        if (!Test.TestTrue(TEXT("Initialize query"), Query.Initialize(
            Fixture.Seed(), Fixture.Minimum(), Fixture.Maximum(), Error))) return false;
        auto Reader = [&Fixture](const FIntPoint& Position, FVoxelHydrologySharedCell& Out, FString& ReadError)
        { return Fixture.Read(Position, Out, ReadError); };
        int64 SortSteps = 0;
        bool bInterrupted = false;
        const int64 Limit = int64(Fixture.Width) * Fixture.Height * 200 + 10000;
        for (int64 Call = 0; Call < Limit; ++Call)
        {
            const bool bSorting = Query.GetProgressStage() / 10 == 4;
            if (bSorting && bInterruptSort && !bInterrupted && SortSteps >= 7)
            {
                const uint64 Bytes = Query.GetAllocatedBytes();
                const int32 Progress = Query.GetProgressStage();
                TAtomic<bool> Cancel(true);
                Test.TestTrue(TEXT("Cancel during partially merged pass"),
                    Query.Advance(Reader, Budget, Error, &Cancel) == EVoxelHydrologyFineWaterStep::Canceled);
                Test.TestEqual(TEXT("Canceled call does not change backing bytes"), Query.GetAllocatedBytes(), Bytes);
                Test.TestEqual(TEXT("Canceled call does not advance stage"), Query.GetProgressStage(), Progress);
                Cancel.Store(false);
                Test.TestTrue(TEXT("Zero budget yields without advancing"),
                    Query.Advance(Reader, 0, Error, &Cancel) == EVoxelHydrologyFineWaterStep::Yielded);
                Test.TestEqual(TEXT("Zero budget leaves backing bytes unchanged"), Query.GetAllocatedBytes(), Bytes);
                bInterrupted = true;
            }
            const uint64 BeforeBytes = Query.GetAllocatedBytes();
            const auto Step = Query.Advance(Reader, Budget, Error);
            if (auto* Scope = FVoxelGenerationAllocationScope::Get())
            {
                if (!Test.TestTrue(TEXT("Retained backing matches both original reservations"),
                    Scope->ValidateRetainedBytes(Query.GetAllocatedBytes(), Error))) return false;
            }
            if (bSorting && Budget == 1)
            {
                ++SortSteps;
                Test.TestEqual(TEXT("Every sort slice including backing swap preserves total bytes"),
                    Query.GetAllocatedBytes(), BeforeBytes);
            }
            if (Step == EVoxelHydrologyFineWaterStep::Ready)
            {
                if (Budget == 1)
                {
                    Test.TestEqual(TEXT("Sort uses merge-only steps, with no per-member copyback"),
                        SortSteps, ExpectedSortSteps(Fixture.Width * Fixture.Height));
                }
                if (bInterruptSort) Test.TestTrue(TEXT("Cancellation path was exercised"), bInterrupted);
                return CheckReady(Test, Fixture, Query);
            }
            if (!Test.TestTrue(FString::Printf(TEXT("Query yields instead of failing: %s"), *Error),
                Step == EVoxelHydrologyFineWaterStep::Yielded)) return false;
        }
        Test.AddError(TEXT("Fixture exceeded bounded completion guard"));
        return false;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineWaterSortSliceTest,
    "WHFramework.Voxel.Hydrology.FineWater.SortSlices",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineWaterSortSliceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    // 覆盖零轮、奇偶轮、未配对尾段及二次幂边界。
    for (const int32 Count : {1, 2, 3, 4, 5, 8, 9, 31, 32, 33})
    {
        FVoxelHydrologyFineWaterQuery Query;
        if (!RunFixture(*this, Query, {Count, 1}, 1, Count == 33)) return false;
        Query.FreeBacking();
        TestEqual(TEXT("All backing released after sorted query"), Query.GetAllocatedBytes(), uint64(0));
    }
    for (const int32 Budget : {1, 7, 4096})
    {
        FVoxelHydrologyFineWaterQuery Query;
        if (!RunFixture(*this, Query, {7, 5}, Budget, false)) return false;
        Query.FreeBacking();
        TestEqual(TEXT("Two-dimensional query releases backing"), Query.GetAllocatedBytes(), uint64(0));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineWaterSortResetTest,
    "WHFramework.Voxel.Hydrology.FineWater.SortCancelReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineWaterSortResetTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FFineWaterSortFixture Fixture{33, 1};
    FVoxelHydrologyFineWaterQuery Query;
    FString Error;
    if (!TestTrue(TEXT("Initialize interrupted query"), Query.Initialize(
        Fixture.Seed(), Fixture.Minimum(), Fixture.Maximum(), Error))) return false;
    auto Reader = [&Fixture](const FIntPoint& Position, FVoxelHydrologySharedCell& Out, FString& ReadError)
    { return Fixture.Read(Position, Out, ReadError); };
    int32 SortSteps = 0;
    // 首次数组交换后，在第二轮归并中间暂停。
    const int32 StopAfter = 33 + 17 + 1 + 7;
    for (int32 Call = 0; Call < 20000 && SortSteps < StopAfter; ++Call)
    {
        if (Query.GetProgressStage() / 10 == 4) ++SortSteps;
        if (!TestTrue(TEXT("Reach second merge pass"), Query.Advance(
            Reader, 1, Error) == EVoxelHydrologyFineWaterStep::Yielded)) return false;
    }
    if (!TestEqual(TEXT("Reached post-swap interruption"), SortSteps, StopAfter)) return false;
    TAtomic<bool> Cancel(true);
    TestTrue(TEXT("Cancel after first backing swap"), Query.Advance(
        Reader, 1, Error, &Cancel) == EVoxelHydrologyFineWaterStep::Canceled);
    Query.FreeBacking();
    TestEqual(TEXT("Canceled mixed buffers fully released"), Query.GetAllocatedBytes(), uint64(0));
    TestFalse(TEXT("Reset clears readiness"), Query.IsReady());
    if (!RunFixture(*this, Query, {9, 3}, 1, false)) return false;
    const TArray<FIntPoint> Taken = Query.TakeCells();
    TestEqual(TEXT("TakeCells transfers the final sorted buffer"), Taken.Num(), 27);
    TestEqual(TEXT("Transferred first cell is sorted"), Taken[0], FIntPoint(-7, -11));
    TestEqual(TEXT("Transferred last cell is sorted"), Taken.Last(), FIntPoint(1, -9));
    Query.FreeBacking();
    TestEqual(TEXT("Reset after transfer releases remaining backing"), Query.GetAllocatedBytes(), uint64(0));
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineWaterSortAccountingTest,
    "WHFramework.Voxel.Hydrology.FineWater.SortAccounting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineWaterSortAccountingTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
    if (!TestTrue(TEXT("Configure ledger"), Resources->Configure({}))) return false;
    const auto Consumer = FVoxelGenerationConsumerKey::Section(37, FGuid::NewGuid(), 1, {}, 1);
    TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working, Resident;
    constexpr uint64 RootBytes = sizeof(FVoxelHydrologyFineWaterQuery);
    if (!TestTrue(TEXT("Acquire working root"),
        Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, RootBytes, Working))) return false;
    if (!TestTrue(TEXT("Acquire resident root"),
        Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident, RootBytes, Resident))) return false;
    {
        FVoxelGenerationAllocationScope Scope(Resources, Consumer, Working, Resident, RootBytes);
        FVoxelHydrologyFineWaterQuery Query;
        FString Error;
        if (!TestTrue(TEXT("Bound scope valid"), Scope.Check(Error))) return false;
        for (const int32 Count : {1, 9, 33})
        {
            const bool bCompleted = RunFixture(*this, Query, {Count, 1}, 1, Count == 33);
            Query.FreeBacking();
            TestEqual(TEXT("Working reservation releases exactly freed buffers"), Working->Bytes(), RootBytes);
            TestEqual(TEXT("Resident reservation releases exactly freed buffers"), Resident->Bytes(), RootBytes);
            TestTrue(TEXT("Empty query reconciles with bound scope"),
                Scope.ValidateRetainedBytes(Query.GetAllocatedBytes(), Error));
            if (!bCompleted) return false;
        }
    }
    Working.Reset();
    Resident.Reset();
    TestEqual(TEXT("No working payload leaked"),
        Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
    TestEqual(TEXT("No resident payload leaked"),
        Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Resident)], uint64(0));
    return true;
}

#endif
