#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyDryWitness.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelDryWitnessNeighbourResumeTest,
    "WHFramework.Voxel.Hydrology.DryWitness.NeighbourResumeAndCancel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelDryWitnessNeighbourResumeTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FVoxelHydrologyDryWitness Witness;
    FString Error;
    const FIntPoint Seed(-1, -1);
    if (!TestTrue(TEXT("初始化负坐标认证"), Witness.Initialize(Seed, {-256, -256}, {256, 256}, Error))) return false;
    int32 SeedReads = 0;
    int32 NeighbourReads = 0;
    int32 HintCalls = 0;
    auto Reader = [&](const FIntPoint& Cell, FVoxelHydrologySharedCell& Out, FString& ReadError)
    {
        Out = FVoxelHydrologySharedCell();
        Out.Cell = Cell;
        Out.GroundZ = 10;
        ReadError.Reset();
        if (Cell == Seed)
        {
            ++SeedReads;
        }
        else
        {
            if (!TestEqual(TEXT("续跑始终重试同一未完成四邻"), Cell, FIntPoint(-2, -1)))
                return EVoxelHydrologyClosureRead::Failed;
            if (++NeighbourReads <= 2)
            {
                ReadError = TEXT("Yielded: certified neighbour input");
                return EVoxelHydrologyClosureRead::Pending;
            }
            Out.GroundZ = 9;
            Out.bHasCertifiedDryOutlet = true;
        }
        return EVoxelHydrologyClosureRead::Ready;
    };
    auto Hint = [&](const FIntPoint&) { ++HintCalls; return false; };
    TestTrue(TEXT("先只读取种子"), Witness.Advance(Reader, Hint, 1, Error) == EVoxelHydrologyDryWitnessStep::Yielded);
    TestTrue(TEXT("四邻缺失保持未完成"), Witness.Advance(Reader, Hint, 1, Error) == EVoxelHydrologyDryWitnessStep::NeedInput);
    TAtomic<bool> Cancel(true);
    TestTrue(TEXT("取消优先于续跑读取"), Witness.Advance(Reader, Hint, 1, Error, &Cancel) == EVoxelHydrologyDryWitnessStep::Canceled);
    TestEqual(TEXT("取消不新增读取"), NeighbourReads, 1);
    Cancel.Store(false);
    TestTrue(TEXT("同一四邻仍未就绪"), Witness.Advance(Reader, Hint, 1, Error, &Cancel) == EVoxelHydrologyDryWitnessStep::NeedInput);
    if (!TestTrue(TEXT("已认证低障碍四邻接续证明"),
        Witness.Advance(Reader, Hint, 1, Error, &Cancel) == EVoxelHydrologyDryWitnessStep::Proven)) return false;
    TestEqual(TEXT("有效种子不重读"), SeedReads, 1);
    TestEqual(TEXT("无需远处候选探测"), HintCalls, 0);
    TestEqual(TEXT("无需搜索图动态正文"), Witness.GetAllocatedBytes(), uint64(0));
    TestTrue(TEXT("只证明当前种子"), Witness.GetCertificate().Cell == Seed && Witness.GetCertificate().bIncluded);
    TestEqual(TEXT("保留种子障碍上界"), Witness.GetCertificate().SpillZ, 10);
    Witness.FreeBacking();
    TestTrue(TEXT("释放后可重新初始化"), Witness.Initialize({1, 1}, {-256, -256}, {256, 256}, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelDryWitnessNeighbourProofTest,
    "WHFramework.Voxel.Hydrology.DryWitness.NeighbourProofBoundaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelDryWitnessNeighbourProofTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    for (int32 Case = 0; Case < 5; ++Case)
    {
        FVoxelHydrologyDryWitness Witness;
        FString Error;
        if (!TestTrue(TEXT("初始化边界案例"), Witness.Initialize({0, 0}, {-256, -256}, {256, 256}, Error))) return false;
        auto Reader = [Case](const FIntPoint& Cell, FVoxelHydrologySharedCell& Out, FString& ReadError)
        {
            Out = FVoxelHydrologySharedCell();
            Out.Cell = Cell;
            Out.GroundZ = Cell == FIntPoint::ZeroValue ? 10 : 11;
            if (Cell == FIntPoint(-1, 0))
            {
                Out.GroundZ = Case == 0 ? 11 : 9;
                Out.bHasCertifiedDryOutlet = Case == 0 || Case == 4;
                if (Case >= 2)
                {
                    Out.GroundZ = 0;
                    Out.ReceiverWaterZ = Case == 2 ? 11 : 10;
                    Out.ReceiverId.High = 1;
                    Out.ReceiverId.Low = 2;
                }
            }
            ReadError.Reset();
            return EVoxelHydrologyClosureRead::Ready;
        };
        auto Hint = [](const FIntPoint&) { return false; };
        EVoxelHydrologyDryWitnessStep Step = EVoxelHydrologyDryWitnessStep::Yielded;
        for (int32 Slice = 0; Slice < 128 && Step == EVoxelHydrologyDryWitnessStep::Yielded; ++Slice)
            Step = Witness.Advance(Reader, Hint, 1, Error);
        if (Case <= 2)
            TestTrue(TEXT("高障碍、未知低干列和高水位都不能证明"), Step == EVoxelHydrologyDryWitnessStep::Unproven);
        else if (Case == 3)
            TestTrue(TEXT("真实湿体水位不高于种子可证明"), Step == EVoxelHydrologyDryWitnessStep::Proven);
        else
        {
            TestTrue(TEXT("混合干湿身份仍被拒绝"), Step == EVoxelHydrologyDryWitnessStep::Failed);
            TestTrue(TEXT("明确保留无效身份错误"), Error.StartsWith(TEXT("Invalid mixed wet/local")));
        }
    }
    return true;
}

#endif
