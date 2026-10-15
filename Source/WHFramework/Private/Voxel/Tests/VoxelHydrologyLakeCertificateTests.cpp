#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyLakeCertificate.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyLakeCertificatePagedTest,
    "WHFramework.Voxel.Hydrology.LakeCertificate.PagedResumeAndCompleteBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyLakeCertificatePagedTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    for (int32 Fixture = 0; Fixture < 3; ++Fixture)
    {
        FVoxelHydrologyLakeCertificate Certificate;
        FVoxelStableId Ocean, Lake;
        Ocean.High = 1;
        Lake.High = 2;
        FVoxelHydrologySharedCell Seed;
        Seed.ReceiverWaterZ = 10;
        Seed.ReceiverId = Lake;
        FString Error;
        const FIntPoint Minimum = Fixture == 2 ? FIntPoint(0, 0) : FIntPoint(-2, -2);
        const FIntPoint Maximum = Fixture == 2 ? FIntPoint(63, 39) : FIntPoint(65, 41);
        if (!TestTrue(TEXT("初始化完整湖岸"), Certificate.Initialize(Seed, Ocean, Minimum, Maximum, Error))) return false;
        TAtomic<bool> InitialCancel(true);
        bool bPending = true;
        auto Reader = [&](const FIntPoint& Cell, FVoxelHydrologySharedCell& Out, FString& ReadError)
        {
            ReadError.Reset();
            if (bPending && Cell == FIntPoint(-1, 0))
            {
                bPending = false;
                return EVoxelHydrologyClosureRead::Pending;
            }
            Out = FVoxelHydrologySharedCell();
            Out.Cell = Cell;
            const bool bInside = Cell.X >= 0 && Cell.X < 64 && Cell.Y >= 0 && Cell.Y < 40;
            Out.GroundZ = bInside ? (Cell.X + Cell.Y) % 3 : 12;
            if (bInside && (Cell.X + Cell.Y) % 17 == 0)
            {
                Out.ReceiverWaterZ = 10;
                Out.ReceiverId = Lake;
            }
            if (Fixture == 1 && Cell == FIntPoint(-1, 0))
            {
                Out.GroundZ = 0;
                Out.ReceiverWaterZ = 5;
                Out.ReceiverId = Ocean;
            }
            return EVoxelHydrologyClosureRead::Ready;
        };
        TestTrue(TEXT("边界否定前也须先响应取消"), Certificate.Advance(Reader, 256, Error, &InitialCancel) == EVoxelHydrologySharedSearchStep::Canceled);
        TestFalse(TEXT("取消不能发布任何结论"), Certificate.IsReady());
        bool bReady = false, bSawPending = false;
        for (int32 Slice = 0; Slice < 20000; ++Slice)
        {
            const auto Step = Certificate.Advance(Reader, 1, Error);
            if (Step == EVoxelHydrologySharedSearchStep::NeedInput)
            {
                bSawPending = true;
                TAtomic<bool> Cancel(true);
                TestTrue(TEXT("待输入时取消仍优先"), Certificate.Advance(Reader, 256, Error, &Cancel) == EVoxelHydrologySharedSearchStep::Canceled);
            }
            else if (Step == EVoxelHydrologySharedSearchStep::Ready) { bReady = true; break; }
            else if (Step != EVoxelHydrologySharedSearchStep::Yielded) { AddError(Error); return false; }
        }
        if (!TestTrue(TEXT("认证或真实拒绝见证完成"), bReady && (Fixture == 2 || bSawPending))) return false;
        if (Fixture == 0)
            TestEqual(TEXT("接受仍需完整跨三个分页的真实湿成员"), Certificate.GetCells().Num(), 2560);
        else
            TestEqual(TEXT("真实出口仅否定实际连通种子，不虚构其余湖岸"), Certificate.GetCells().Num(), 1);
        TestEqual(TEXT("低水位接触与开放边界仍拒绝湖根"), Certificate.IsAccepted(), Fixture == 0);
        const auto Cells = Certificate.GetCells();
        for (int32 Index = 0; Index < Cells.Num(); ++Index)
        {
            const auto Cell = Cells[Index];
            const bool bReceiver = (Cell.Cell.X + Cell.Cell.Y) % 17 == 0;
            TestEqual(TEXT("视图保留每格原地面"), Cell.GroundZ, (Cell.Cell.X + Cell.Cell.Y) % 3);
            TestEqual(TEXT("非接收成员未被升级为湖根"), Cell.IsReceiver(), bReceiver);
            if (bReceiver)
                TestTrue(TEXT("接收根水位与原身份不变"), Cell.ReceiverWaterZ == 10 && Cell.ReceiverId == Lake);
        }
        Certificate.FreeBacking();
        TestEqual(TEXT("分页和访问集正文释放"), Certificate.GetAllocatedBytes(), uint64(0));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyLakeCertificateBitmapTest,
    "WHFramework.Voxel.Hydrology.LakeCertificate.NegativeBitmapAndReceiverIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyLakeCertificateBitmapTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FIntPoint Origin(-65, -33);
    auto Sample = [&](const FIntPoint& Position)
    {
        FVoxelHydrologySharedCell Value;
        Value.Cell = Position;
        const FIntPoint Local = Position - Origin;
        const bool bInside = Local.X >= 0 && Local.X < 128 && Local.Y >= 0 && Local.Y < 96;
        Value.GroundZ = bInside ? 4 + (Local.X + Local.Y) % 3 : 15;
        if (bInside)
        {
            const int32 Index = Local.X + Local.Y * 128;
            if (Index % 5 == 0 || Index % 7 == 0)
            {
                Value.ReceiverWaterZ = Index % 5 == 0 ? 10 : 12;
                Value.ReceiverId.High = 5;
                Value.ReceiverId.Low = uint64(Index % 173);
            }
        }
        return Value;
    };
    FVoxelStableId Ocean;
    Ocean.High = 1;
    FVoxelHydrologyLakeCertificate Certificate;
    FString Error;
    if (!TestTrue(TEXT("负坐标跨块封闭湖初始化"), Certificate.Initialize(Sample(Origin), Ocean,
        Origin - FIntPoint(2, 2), Origin + FIntPoint(129, 97), Error))) return false;
    TSet<FIntPoint> ReadyReads;
    bool bPending = true;
    auto Reader = [&](const FIntPoint& Position, FVoxelHydrologySharedCell& Out, FString& ReadError)
    {
        ReadError.Reset();
        if (bPending && Position == FIntPoint(-32, -32))
        {
            bPending = false;
            return EVoxelHydrologyClosureRead::Pending;
        }
        TestFalse(TEXT("位图无别名且成功读取不重放"), ReadyReads.Contains(Position));
        ReadyReads.Add(Position);
        Out = Sample(Position);
        return EVoxelHydrologyClosureRead::Ready;
    };
    bool bReady = false;
    for (int32 Slice = 0; Slice < 1024; ++Slice)
    {
        const auto Step = Certificate.Advance(Reader, 257, Error);
        if (Step == EVoxelHydrologySharedSearchStep::Ready) { bReady = true; break; }
        if (Step != EVoxelHydrologySharedSearchStep::Yielded && Step != EVoxelHydrologySharedSearchStep::NeedInput)
        { AddError(Error); return false; }
    }
    if (!TestTrue(TEXT("真实Pending后完整闭合"), bReady && !bPending && Certificate.IsAccepted())) return false;
    const auto Cells = Certificate.GetCells();
    TestEqual(TEXT("所有12288个真实成员完整保留"), Cells.Num(), 128 * 96);
    TestTrue(TEXT("正文及全部位图/身份元数据低于512KiB"), Certificate.GetAllocatedBytes() < uint64(512) * 1024);
    TSet<FIntPoint> Members;
    for (int32 Index = 0; Index < Cells.Num(); ++Index)
    {
        const auto Actual = Cells[Index];
        const auto Expected = Sample(Actual.Cell);
        TestFalse(TEXT("成员不因位图跨块而重复"), Members.Contains(Actual.Cell));
        Members.Add(Actual.Cell);
        TestEqual(TEXT("原始地面完整还原"), Actual.GroundZ, Expected.GroundZ);
        TestEqual(TEXT("相同ID不同水位仍各自还原"), Actual.ReceiverWaterZ, Expected.ReceiverWaterZ);
        TestTrue(TEXT("不同接收体完整身份不被合并"), Actual.ReceiverId == Expected.ReceiverId);
    }
    TestTrue(TEXT("保留完整规范组身份"), Certificate.GetGroupId() == Sample(Origin).ReceiverId);
    Certificate.FreeBacking();
    TestEqual(TEXT("全部新正文及元数据释放"), Certificate.GetAllocatedBytes(), uint64(0));
    return true;
}

#endif
