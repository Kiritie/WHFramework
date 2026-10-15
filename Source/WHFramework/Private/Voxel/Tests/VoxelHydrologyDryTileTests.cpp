#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyFineWaterQuery.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"

namespace VoxelDryTileTests
{
    bool Seed(FAutomationTestBase& Test, FVoxelHydrologyFineWaterQuery& Query, const FIntPoint& Cell,
        int32 Ground, const FIntPoint& Receiver, const FIntPoint& Minimum, const FIntPoint& Maximum)
    {
        FString Error;
        if (!Test.TestTrue(TEXT("初始化真实干种子查询"), Query.Initialize(Cell, Minimum, Maximum, Error))) return false;
        auto Reader = [&](const FIntPoint& Position, FVoxelHydrologySharedCell& Out, FString& ReadError)
        {
            Out = FVoxelHydrologySharedCell();
            Out.Cell = Position;
            Out.GroundZ = Ground;
            if (Position == Receiver)
            {
                Out.GroundZ = Ground - 2;
                Out.ReceiverWaterZ = Ground - 1;
                Out.ReceiverId.High = 1;
                Out.ReceiverId.Low = 2;
            }
            ReadError.Reset();
            return EVoxelHydrologyClosureRead::Ready;
        };
        const FVoxelHydrologyFineWaterQuery::FWetHint Hint = [](const FIntPoint&) { return false; };
        for (int32 Slice = 0; Slice < 16; ++Slice)
        {
            const auto Step = Query.Advance(Reader, 1, Error, nullptr, &Hint);
            if (Step == EVoxelHydrologyFineWaterStep::Ready)
                return Test.TestFalse(TEXT("已认证种子不含水"), Query.GetResult().IsWet());
            if (Step != EVoxelHydrologyFineWaterStep::Yielded) { Test.AddError(Error); return false; }
        }
        Test.AddError(TEXT("种子未在有界步骤内得到真实接续证明"));
        return false;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyDryTileResumeTest,
    "WHFramework.Voxel.Hydrology.DryTile.ProofResumeAndExclusions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyDryTileResumeTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FVoxelHydrologyFineWaterQuery Query;
    if (!VoxelDryTileTests::Seed(*this, Query, {0, 0}, 1, {-1, 0}, {-1, -1}, {15, 15})) return false;
    int32 PendingReads = 0;
    auto Reader = [&](const FIntPoint& Position, FVoxelHydrologySharedCell& Out, FString& Error)
    {
        Out = FVoxelHydrologySharedCell();
        Out.Cell = Position;
        Out.GroundZ = Position == FIntPoint(3, 3) ? 0 : 1;
        Error.Reset();
        if (Position == FIntPoint(1, 0) && ++PendingReads <= 2) return EVoxelHydrologyClosureRead::Pending;
        if (Position == FIntPoint(2, 2))
        {
            Out.GroundZ = -1;
            Out.ReceiverWaterZ = 0;
            Out.ReceiverId.High = 1;
            Out.ReceiverId.Low = 2;
        }
        return EVoxelHydrologyClosureRead::Ready;
    };
    FString Error;
    TAtomic<bool> Cancel(false);
    bool bCanceledOnce = false;
    bool bReady = false;
    for (int32 Slice = 0; Slice < 4096; ++Slice)
    {
        const auto Step = Query.ExpandCertifiedDryTile(Reader, 1, Error, &Cancel);
        if (Step == EVoxelHydrologyFineWaterStep::NeedTiles && !bCanceledOnce)
        {
            const int32 Count = Query.GetDrainageCertificate().Num();
            Cancel.Store(true);
            TestTrue(TEXT("取消优先且不推进证明"), Query.ExpandCertifiedDryTile(Reader, 256, Error, &Cancel) == EVoxelHydrologyFineWaterStep::Canceled);
            TestEqual(TEXT("取消保留已完成前缀"), Query.GetDrainageCertificate().Num(), Count);
            Cancel.Store(false);
            bCanceledOnce = true;
        }
        else if (Step == EVoxelHydrologyFineWaterStep::Ready) { bReady = true; break; }
        else if (Step != EVoxelHydrologyFineWaterStep::Yielded && Step != EVoxelHydrologyFineWaterStep::NeedTiles)
        { AddError(Error); return false; }
    }
    if (!TestTrue(TEXT("单步续跑最终完成"), bReady && bCanceledOnce)) return false;
    const auto& Certificates = Query.GetDrainageCertificate();
    TestEqual(TEXT("仅254个逐格可证明坐标被产出"), Certificates.Num(), 254);
    for (const FIntPoint Excluded : {FIntPoint(2, 2), FIntPoint(3, 3), FIntPoint(16, 0), FIntPoint(-1, 0)})
        TestFalse(TEXT("水体、低洼与瓦片外均没有被整块认定"), Certificates.ContainsByPredicate(
            [&](const FVoxelHydrologyFineWaterCertificate& Value) { return Value.Cell == Excluded; }));
    TestTrue(TEXT("瓦片远角由有效路径得到证明"), Certificates.ContainsByPredicate(
        [](const FVoxelHydrologyFineWaterCertificate& Value) { return Value.Cell == FIntPoint(15, 15); }));
    for (const auto& Certificate : Certificates)
        TestTrue(TEXT("每格保留原干证明不变量"), Certificate.bIncluded && !Certificate.IsReceiver() && Certificate.GroundZ >= Certificate.SpillZ);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyDryTileAllocationTest,
    "WHFramework.Voxel.Hydrology.DryTile.NegativeBoundsAndNativeRelease",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyDryTileAllocationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
    if (!TestTrue(TEXT("配置原账本"), Resources->Configure({}))) return false;
    const auto Consumer = FVoxelGenerationConsumerKey::Section(37, FGuid::NewGuid(), 1, {}, 1);
    TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
    TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Resident;
    const uint64 Root = sizeof(FVoxelHydrologyFineWaterQuery);
    if (!TestTrue(TEXT("原工作和驻留租约先授额"),
        Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, Root, Working) &&
        Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident, Root, Resident))) return false;
    {
        FVoxelGenerationAllocationScope Allocation(Resources, Consumer, Working, Resident, Root);
        FVoxelHydrologyFineWaterQuery Query;
        FString Error;
        for (int32 Pass = 0; Pass < 2; ++Pass)
        {
            const bool bNegative = Pass == 0;
            if (!VoxelDryTileTests::Seed(*this, Query, bNegative ? FIntPoint(-1, -1) : FIntPoint(0, 0), 2,
                bNegative ? FIntPoint(0, -1) : FIntPoint(-1, 0), bNegative ? FIntPoint(-8, -8) : FIntPoint(-1, -1),
                bNegative ? FIntPoint(0, 0) : FIntPoint(15, 15))) return false;
            auto Reader = [](const FIntPoint& Cell, FVoxelHydrologySharedCell& Out, FString& ReadError)
            {
                Out = FVoxelHydrologySharedCell();
                Out.Cell = Cell;
                Out.GroundZ = 2;
                ReadError.Reset();
                return EVoxelHydrologyClosureRead::Ready;
            };
            EVoxelHydrologyFineWaterStep Step = EVoxelHydrologyFineWaterStep::Yielded;
            for (int32 Slice = 0; Slice < 4096 && Step == EVoxelHydrologyFineWaterStep::Yielded; ++Slice)
                Step = Query.ExpandCertifiedDryTile(Reader, 1, Error);
            if (!TestTrue(TEXT("有界证明完成"), Step == EVoxelHydrologyFineWaterStep::Ready)) return false;
            TestEqual(TEXT("负瓦片按真实世界边界裁剪，重置后正瓦片完整证明"),
                Query.GetDrainageCertificate().Num(), bNegative ? 64 : 256);
            TestTrue(TEXT("实际动态正文与原授额一致"), Allocation.ValidateRetainedBytes(Query.GetAllocatedBytes(), Error));
            Query.FreeBacking();
            TestEqual(TEXT("释放后工作额度回到根"), Working->Bytes(), Root);
            TestEqual(TEXT("释放后驻留额度回到根"), Resident->Bytes(), Root);
        }
    }
    Working.Reset();
    Resident.Reset();
    TestEqual(TEXT("工作正文无泄漏"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
    TestEqual(TEXT("驻留正文无泄漏"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Resident)], uint64(0));
    return true;
}

#endif
