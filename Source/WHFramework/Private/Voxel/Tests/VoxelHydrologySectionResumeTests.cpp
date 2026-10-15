#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyJunctionCertificate.h"

namespace
{
    FVoxelRiverShape ResumeShape()
    {
        FVoxelRiverShape Shape;
        Shape.BedHalfWidth = Shape.WaterHalfWidth = Shape.BankWidth = Shape.ShoreWidth = Shape.Depth = 1;
        Shape.MaxCutFill = 8;
        return Shape;
    }

    FVoxelHydrologySectionSample SampleAt(const FIntPoint& P, int32 Scenario)
    {
        FVoxelHydrologySectionSample S;
        S.OriginalGround = 10;
        if (Scenario > 0 && (P == FIntPoint(0, 0) || P == FIntPoint(1, 0)))
        {
            S.OriginalGround = 7;
            S.ExistingWaterZ = 9;
            S.ExistingBodyId.High = 1;
            S.ExistingBodyId.Low = P.X == 0 ? 2 : 1;
        }
        if (Scenario == 2 && P == FIntPoint(0, 1))
        {
            S.OriginalGround = 7;
            S.ExistingWaterZ = 10;
            S.ExistingBodyId.High = 1;
            S.ExistingBodyId.Low = 3;
        }
        return S;
    }

    bool SameInterval(const FVoxelHydrologySectionInterval& A, const FVoxelHydrologySectionInterval& B)
    {
        return A.LowerWaterZ == B.LowerWaterZ && A.UpperWaterZ == B.UpperWaterZ &&
            A.FixedWaterZ == B.FixedWaterZ && A.FixedBodyId == B.FixedBodyId &&
            A.FixedConnector == B.FixedConnector && A.bEligible == B.bEligible;
    }

    bool RunSection(FVoxelHydrologySectionCertificate& Section,
        FVoxelHydrologySectionCertificate::FReadSample Read, int32 Budget, FString& Error)
    {
        for (int32 Attempt = 0; Attempt < 10000; ++Attempt)
        {
            const auto Step = Section.Advance(Read, Budget, Error);
            if (Step == EVoxelHydrologySharedSearchStep::Ready) return true;
            if (Step != EVoxelHydrologySharedSearchStep::Yielded &&
                Step != EVoxelHydrologySharedSearchStep::NeedInput &&
                Step != EVoxelHydrologySharedSearchStep::CapacityBlocked) return false;
        }
        Error = TEXT("Test iteration limit reached");
        return false;
    }

    // 逐个完整断面组合原连接几何，不使用跨断面缓存。
    bool ReferenceJunction(const FVoxelRiverShape& Shape, int32 Scenario,
        FVoxelHydrologySectionInterval& Result, FIntPoint& ConnectorEnd, int32& Reads, FString& Error)
    {
        const int32 Half = Shape.WaterHalfWidth + Shape.BankWidth + Shape.ShoreWidth + 1;
        static const FIntPoint Directions[] = {FIntPoint(-1,0), FIntPoint(1,0), FIntPoint(0,-1), FIntPoint(0,1)};
        Result = FVoxelHydrologySectionInterval();
        Result.LowerWaterZ = Shape.Depth;
        Result.UpperWaterZ = 32766;
        bool bIneligible = false;
        for (int32 Index = 0; Index <= 4 * Half; ++Index)
        {
            FIntPoint Position(0, 0);
            if (Index > 0) Position = Directions[(Index - 1) / Half] * ((Index - 1) % Half + 1);
            FVoxelHydrologySectionCertificate Section;
            if (!Section.Initialize(Position, Shape, 0, FIntPoint(-32, -32), FIntPoint(32, 32), Error)) return false;
            auto Reader = [&](const FIntPoint& P, FVoxelHydrologySectionSample& S, FString& E)
            { ++Reads; S = SampleAt(P, Scenario); E.Reset(); return EVoxelHydrologyClosureRead::Ready; };
            if (!RunSection(Section, Reader, 1, Error)) return false;
            const auto& Interval = Section.GetResult();
            bIneligible |= !Interval.bEligible;
            Result.LowerWaterZ = FMath::Max(Result.LowerWaterZ, Interval.LowerWaterZ);
            Result.UpperWaterZ = FMath::Min(Result.UpperWaterZ, Interval.UpperWaterZ);
            if (Interval.FixedWaterZ != MIN_int32)
            {
                bIneligible |= Result.FixedWaterZ != MIN_int32 && Result.FixedWaterZ != Interval.FixedWaterZ;
                const bool Better = Result.FixedWaterZ == MIN_int32 || Interval.FixedBodyId < Result.FixedBodyId ||
                    (Interval.FixedBodyId == Result.FixedBodyId && (Position.X < ConnectorEnd.X ||
                     (Position.X == ConnectorEnd.X && Position.Y < ConnectorEnd.Y)));
                if (Better)
                {
                    Result.FixedBodyId = Interval.FixedBodyId;
                    Result.FixedConnector = Interval.FixedConnector;
                    ConnectorEnd = Position;
                }
                Result.FixedWaterZ = Interval.FixedWaterZ;
            }
        }
        Result.bEligible = !bIneligible && Result.LowerWaterZ <= Result.UpperWaterZ;
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologySectionResumeTest,
    "WHFramework.Voxel.Hydrology.Section.ResumeEveryRead",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologySectionResumeTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const auto Shape = ResumeShape();
    // 每次读取先中断一次；成功读序列必须与不中断的完整认证一致。
    for (int32 Scenario = 0; Scenario < 4; ++Scenario)
    {
        const FIntPoint WorldMin = Scenario == 3 ? FIntPoint(-3, -3) : FIntPoint(-32, -32);
        const FIntPoint WorldMax = Scenario == 3 ? FIntPoint(3, 3) : FIntPoint(32, 32);
        FString Error;
        FVoxelHydrologySectionCertificate Baseline;
        if (!TestTrue(TEXT("Initialize baseline"), Baseline.Initialize({}, Shape, 0, WorldMin, WorldMax, Error))) return false;
        TArray<FIntPoint> Trace;
        auto Immediate = [&](const FIntPoint& P, FVoxelHydrologySectionSample& S, FString& E)
        { Trace.Add(P); S = SampleAt(P, Scenario); E.Reset(); return EVoxelHydrologyClosureRead::Ready; };
        if (!TestTrue(TEXT("Complete baseline"), RunSection(Baseline, Immediate, 1, Error))) return false;
        if (Scenario == 0) TestTrue(TEXT("Dry interior remains eligible"), Baseline.GetResult().bEligible);
        if (Scenario == 2 || Scenario == 3) TestFalse(TEXT("Conflict/open footprint stays ineligible"), Baseline.GetResult().bEligible);
        for (int32 Kind = 0; Kind < 5; ++Kind)
        {
            FVoxelHydrologySectionCertificate Resumed;
            if (!TestTrue(TEXT("Initialize resumed"), Resumed.Initialize({}, Shape, 0, WorldMin, WorldMax, Error))) return false;
            int32 Cursor = 0;
            bool bPending = true;
            bool bTraceMatches = true;
            auto Interrupted = [&](const FIntPoint& P, FVoxelHydrologySectionSample& S, FString& E)
            {
                if (!Trace.IsValidIndex(Cursor) || Trace[Cursor] != P)
                { bTraceMatches = false; E = TEXT("Reader restarted a completed operation"); return EVoxelHydrologyClosureRead::Failed; }
                if (bPending)
                {
                    bPending = false;
                    // 未就绪返回中的无效输出不得被保留。
                    S.OriginalGround = MAX_int32;
                    E = Kind == 0 ? TEXT("") : (Kind == 1 || Kind == 3) ? TEXT("Yielded: injected") : TEXT("CapacityBlocked: injected");
                    return Kind < 3 ? EVoxelHydrologyClosureRead::Pending : EVoxelHydrologyClosureRead::Failed;
                }
                bPending = true; ++Cursor;
                S = SampleAt(P, Scenario); E.Reset();
                return EVoxelHydrologyClosureRead::Ready;
            };
            const bool bFinished = RunSection(Resumed, Interrupted, 1, Error);
            TestTrue(TEXT("Resume completes with no replay of successful reads"), bFinished && bTraceMatches);
            if (!bFinished) { AddError(Error); return false; }
            TestEqual(TEXT("Every original read is still certified"), Cursor, Trace.Num());
            TestTrue(TEXT("Water/body/connector/interval preserved"), SameInterval(Baseline.GetResult(), Resumed.GetResult()));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyJunctionResumeTest,
    "WHFramework.Voxel.Hydrology.Junction.SectionResumeEquivalence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyJunctionResumeTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const auto Shape = ResumeShape();
    FVoxelHydrologyJunctionCertificate Junction;
    for (int32 Scenario = 0; Scenario < 3; ++Scenario)
    {
        FString Error;
        FVoxelHydrologySectionInterval Expected;
        FIntPoint ExpectedEnd(0, 0);
        int32 ReferenceReads = 0;
        if (!TestTrue(TEXT("Reference complete geometry"), ReferenceJunction(Shape, Scenario, Expected, ExpectedEnd, ReferenceReads, Error))) return false;
        if (!TestTrue(TEXT("Initialize new immutable input"), Junction.Initialize({}, Shape, 0, FIntPoint(-32,-32), FIntPoint(32,32), Error))) return false;
        int32 SuccessfulReads = 0;
        bool bPending = true;
        bool bReady = false;
        auto Read = [&](const FIntPoint& P, FVoxelHydrologySectionSample& S, FString& E)
        {
            if (bPending)
            { bPending = false; S.OriginalGround = MAX_int32; E = TEXT("Yielded: injected"); return EVoxelHydrologyClosureRead::Pending; }
            bPending = true; ++SuccessfulReads; S = SampleAt(P, Scenario); E.Reset();
            return EVoxelHydrologyClosureRead::Ready;
        };
        for (int32 Attempt = 0; Attempt < 20000; ++Attempt)
        {
            const auto Step = Junction.Advance(Read, 1, Error);
            if (Step == EVoxelHydrologySharedSearchStep::Ready) { bReady = true; break; }
            if (Step != EVoxelHydrologySharedSearchStep::Yielded)
            { AddError(Error); break; }
        }
        if (!TestTrue(TEXT("Complete junction under per-read interruptions"), bReady)) { Junction.Reset(); return false; }
        TestTrue(TEXT("Exact complete-section result"), SameInterval(Expected, Junction.GetResult()));
        TestTrue(TEXT("Exact connector end"), Expected.FixedWaterZ == MIN_int32 || ExpectedEnd == Junction.GetConnectorEnd());
        TestEqual(TEXT("No extra successful source reads due to reentry"), SuccessfulReads, ReferenceReads);
        Junction.Reset();
        Junction.Reset();
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologySectionCancelInvalidTest,
    "WHFramework.Voxel.Hydrology.Section.CancelInvalidAndReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologySectionCancelInvalidTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FString Error;
    FVoxelHydrologySectionCertificate Section;
    const auto Shape = ResumeShape();
    if (!TestTrue(TEXT("Initialize"), Section.Initialize({}, Shape, 0, FIntPoint(-32,-32), FIntPoint(32,32), Error))) return false;
    int32 Calls = 0;
    auto PendingNeighbour = [&](const FIntPoint& P, FVoxelHydrologySectionSample& S, FString& E)
    {
        ++Calls; S = SampleAt(P, 0);
        if (Calls == 2) { E.Reset(); return EVoxelHydrologyClosureRead::Pending; }
        E.Reset(); return EVoxelHydrologyClosureRead::Ready;
    };
    TestTrue(TEXT("Zero budget yields without input"), Section.Advance(PendingNeighbour, 0, Error) == EVoxelHydrologySharedSearchStep::Yielded);
    TestEqual(TEXT("Zero budget makes no calls"), Calls, 0);
    TestTrue(TEXT("Suspend in first neighbour"), Section.Advance(PendingNeighbour, 100, Error) == EVoxelHydrologySharedSearchStep::NeedInput);
    TAtomic<bool> Cancel(true);
    const int32 Before = Calls;
    TestTrue(TEXT("Cancel wins before any source work"), Section.Advance(PendingNeighbour, 100, Error, &Cancel) == EVoxelHydrologySharedSearchStep::Canceled);
    TestEqual(TEXT("Canceled reader is not invoked"), Calls, Before);
    Section.Reset();
    if (!TestTrue(TEXT("New generation initializes"), Section.Initialize({}, Shape, 0, FIntPoint(-32,-32), FIntPoint(32,32), Error))) return false;
    Calls = 0;
    auto Invalid = [&](const FIntPoint&, FVoxelHydrologySectionSample& S, FString& E)
    {
        ++Calls; S = FVoxelHydrologySectionSample(); S.OriginalGround = 7;
        S.ExistingWaterZ = 9; // 零水体标识无效。
        E.Reset(); return EVoxelHydrologyClosureRead::Ready;
    };
    TestTrue(TEXT("New generation cannot reuse old sample"), Section.Advance(Invalid, 100, Error) == EVoxelHydrologySharedSearchStep::Failed);
    TestEqual(TEXT("Invalid new input read once"), Calls, 1);
    TestFalse(TEXT("Invalid input never publishes"), Section.IsReady());
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologySectionRecentSamplesTest,
    "WHFramework.Voxel.Hydrology.Section.InvocationLocalSampleReuse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologySectionRecentSamplesTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const auto Shape = ResumeShape();
    for (int32 Scenario = 0; Scenario < 4; ++Scenario)
    {
        FString Error;
        const FIntPoint Min = Scenario == 3 ? FIntPoint(-3,-3) : FIntPoint(-32,-32);
        const FIntPoint Max = Scenario == 3 ? FIntPoint(3,3) : FIntPoint(32,32);
        FVoxelHydrologySectionCertificate SingleCell, Batched;
        if (!TestTrue(TEXT("Initialize isolated cells"), SingleCell.Initialize({}, Shape, 0, Min, Max, Error)) ||
            !TestTrue(TEXT("Initialize batched cells"), Batched.Initialize({}, Shape, 0, Min, Max, Error))) return false;
        int32 IsolatedReads = 0, BatchedReads = 0;
        auto IsolatedReader = [&](const FIntPoint& P, FVoxelHydrologySectionSample& S, FString& E)
        { ++IsolatedReads; S = SampleAt(P, Scenario); E.Reset(); return EVoxelHydrologyClosureRead::Ready; };
        auto BatchedReader = [&](const FIntPoint& P, FVoxelHydrologySectionSample& S, FString& E)
        { ++BatchedReads; S = SampleAt(P, Scenario); E.Reset(); return EVoxelHydrologyClosureRead::Ready; };
        // 单格调用不跨格复用；批处理可复用已就绪采样，但必须保留全部认证约束。
        if (!TestTrue(TEXT("Isolated reference completes"), RunSection(SingleCell, IsolatedReader, 1, Error)) ||
            !TestTrue(TEXT("Batched certificate completes"), RunSection(Batched, BatchedReader, 10000, Error))) return false;
        TestTrue(TEXT("Invocation cache preserves exact interval and receiving identity"),
            SameInterval(SingleCell.GetResult(), Batched.GetResult()));
        TestTrue(TEXT("Nearby repeated reads are reduced"), BatchedReads < IsolatedReads);
    }

    // 采样回调可发起取消；下一次读取即使命中栈缓存，也必须先检查取消。
    FString Error;
    FVoxelHydrologySectionCertificate Canceled;
    if (!TestTrue(TEXT("Initialize cancellation case"), Canceled.Initialize({}, Shape, 0, FIntPoint(-32,-32), FIntPoint(32,32), Error))) return false;
    TAtomic<bool> Cancel(false);
    int32 Reads = 0;
    auto Reader = [&](const FIntPoint& P, FVoxelHydrologySectionSample& S, FString& E)
    { ++Reads; S = SampleAt(P, 0); E.Reset(); Cancel.Store(true); return EVoxelHydrologyClosureRead::Ready; };
    TestTrue(TEXT("Cancellation interrupts batch"), Canceled.Advance(Reader, 10000, Error, &Cancel) == EVoxelHydrologySharedSearchStep::Canceled);
    TestEqual(TEXT("No source read after cancellation"), Reads, 1);
    TestFalse(TEXT("Canceled batch does not publish"), Canceled.IsReady());
    Canceled.Reset();
    return true;
}
#endif
