#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "VoxelTestUtilities.h"
#include "Voxel/Streaming/VoxelFineResidency.h"
#include "Voxel/Rendering/VoxelFinePageLayout.h"
#include "Voxel/Rendering/VoxelFineBuildState.h"
#include "Voxel/Rendering/VoxelFineShardBuild.h"
#include "Voxel/Rendering/VoxelFineHandoff.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Task/VoxelTerrainWorkSelection.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineResidencyWindowTest,
    "WHFramework.Voxel.FineRefactor.ResidencyWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineResidencyWindowTest::RunTest(const FString& InParameters)
{
    FString Error;
    FVoxelFineResidency Residency;
    if (!TestTrue(TEXT("初始化固定窗口"), Residency.Initialize(FGuid::NewGuid(), {}, FIntVector::ZeroValue,
        32, 64, 80, -512, 512, Error)))
    {
        AddError(Error);
        return false;
    }
    const auto Original = Residency.GetCurrent();
    FVoxelFineMovementDelta Movement;
    TestTrue(TEXT("同一 Section 内移动"), Residency.UpdateMovement(FIntVector(7, 0, 0), FVector::ZeroVector, 0.0, Movement, Error));
    TestFalse(TEXT("同一 Section 不改变 F0 集合"), Movement.bCoreChanged);
    TestTrue(TEXT("越过 F0 Section 边界"), Residency.UpdateMovement(FIntVector(16, 0, 0), FVector::ZeroVector, 0.0, Movement, Error));
    TestTrue(TEXT("只提交 Added 和 Removed，保留交集"), !Movement.F0.Added.IsEmpty() && !Movement.F0.Retained.IsEmpty());
    TestEqual(TEXT("窗口 Anchor 不跟随 F0"), Residency.GetCurrent().Anchor, Original.Anchor);
    TestTrue(TEXT("接近窗口边界"), Residency.UpdateMovement(FIntVector(144, 0, 0), FVector::ZeroVector, 0.0, Movement, Error));
    FVoxelFineMigrationDecision Decision;
    const FVoxelFineSafetyContext SafetyContext{1, 1, FIntVector::ZeroValue, 0.0};
    const auto CommittedCoverage = [](const FIntVector&)
    {
        return FVoxelFineSectionCoverage{1, 1, true, true, true};
    };
    if (!TestTrue(TEXT("建立 Pending"), Residency.PrepareMigration(FIntVector(144, 0, 0), FVector::ZeroVector,
        0.5, SafetyContext, CommittedCoverage, Decision, Error)))
    {
        AddError(Error);
        return false;
    }
    TestTrue(TEXT("Pending 已建立"), Decision.bStartedPending);
    TestTrue(TEXT("Pending 创建不能释放 Current 尾部"), Decision.Residency.Removed.IsEmpty());
    const uint64 Serial = Residency.GetPending()->Serial;
    TVoxelFineSetDelta<FVoxelFineShardKey> Committed;
    TestFalse(TEXT("只有 MeshReady 不允许交接"), Residency.CommitPending(Serial,
        [](const FVoxelFineShardKey&) { return FVoxelFineShardReadiness{true, true, false}; },
        [](const FIntVector&) { return true; }, SafetyContext, CommittedCoverage, Committed, Error));
    TestEqual(TEXT("Current 持续有效"), Residency.GetCurrent().Serial, Original.Serial);
    TestTrue(TEXT("覆盖与碰撞都已发布才交接"), Residency.CommitPending(Serial,
        [](const FVoxelFineShardKey&) { return FVoxelFineShardReadiness{true, true, true}; },
        [](const FIntVector&) { return true; }, SafetyContext, CommittedCoverage, Committed, Error));
    TestTrue(TEXT("交接不重复加入已经预热的区域"), Committed.Added.IsEmpty());
    TestEqual(TEXT("只退役旧尾部"), Committed.Removed.Num(), Decision.WindowDifference.Removed.Num());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineTravelCoverageTest,
    "WHFramework.Voxel.FineRefactor.CommittedTravelCoverage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineTravelCoverageTest::RunTest(const FString& InParameters)
{
    FString Error;
    FVoxelFineResidency Residency;
    if (!TestTrue(TEXT("初始化安全覆盖用例"), Residency.Initialize(FGuid::NewGuid(), {}, FIntVector::ZeroValue,
        32, 64, 80, -512, 512, Error)))
    {
        return false;
    }
    const FVoxelGenerationBounds CommittedBounds = Residency.GetCurrent().Bounds;
    const auto CommittedCoverage = [&CommittedBounds](const FIntVector& InSection)
    {
        const FVoxelGenerationBounds SectionBounds{InSection * 16, (InSection + FIntVector(1)) * 16};
        const bool bCovered = FVoxelFineSpatial::ContainsBounds(CommittedBounds, SectionBounds);
        return FVoxelFineSectionCoverage{1, 1, bCovered, bCovered, true};
    };
    FVoxelFineSafetyContext SafetyContext{1, 1, FIntVector::ZeroValue, 0.0};
    FVoxelFineMovementDelta Movement;
    FVoxelFineMigrationDecision Decision;
    TestTrue(TEXT("先准备邻窗"), Residency.UpdateMovement(FIntVector(144, 0, 0), FVector::ZeroVector, 0.0, Movement, Error));
    TestTrue(TEXT("产生尚未提交Pending"), Residency.PrepareMigration(FIntVector(144, 0, 0), FVector::ZeroVector,
        0.5, SafetyContext, CommittedCoverage, Decision, Error));
    TestTrue(TEXT("Pending已存在"), Residency.GetPending().IsSet());
    TestTrue(TEXT("提出进入Pending的移动"), Residency.UpdateMovement(FIntVector(300, 0, 0), FVector::ZeroVector, 0.0, Movement, Error));
    TestTrue(TEXT("计算Pending外延安全性"), Residency.PrepareMigration(FIntVector(300, 0, 0), FVector::ZeroVector,
        0.5, SafetyContext, CommittedCoverage, Decision, Error));
    TestTrue(TEXT("Pending需求不能代替已提交覆盖"), Decision.bRequiresTravelBarrier);
    TestTrue(TEXT("中心回到Current范围内"), Residency.UpdateMovement(FIntVector(250, 0, 0), FVector::ZeroVector, 0.0, Movement, Error));
    TestTrue(TEXT("中心几何位置仍在Current内"), CommittedBounds.Contains(FIntVector(250, 0, 0)));
    TestTrue(TEXT("计算整个Guard"), Residency.PrepareMigration(FIntVector(250, 0, 0), FVector::ZeroVector,
        0.5, SafetyContext, CommittedCoverage, Decision, Error));
    TestTrue(TEXT("中心安全但Guard越界仍阻挡"), Decision.bRequiresTravelBarrier);
    TestFalse(TEXT("Guard不具有完整服务证明"), Decision.Safety.bWholeGuardCovered);
    const auto VisualWithoutCollision = [](const FIntVector&)
    {
        return FVoxelFineSectionCoverage{1, 1, true, false, true};
    };
    FVoxelFineSafetyResult Safety;
    TestTrue(TEXT("查询视觉已提交而碰撞未提交"), Residency.EvaluateSafety(SafetyContext, VisualWithoutCollision, Safety, Error));
    TestFalse(TEXT("只有视觉不能构成Safe"), Safety.IsSafe());
    const auto ReadyCommittedCoverage = [](const FIntVector&)
    {
        return FVoxelFineSectionCoverage{1, 1, true, true, true};
    };
    TestTrue(TEXT("查询真实提交区域"), Residency.EvaluateSafety(SafetyContext, ReadyCommittedCoverage, Safety, Error));
    TestTrue(TEXT("整个所需区域真实提交后可以服务"), Safety.IsSafe());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineRevisionIsolationTest,
    "WHFramework.Voxel.FineRefactor.RevisionIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineRevisionIsolationTest::RunTest(const FString& InParameters)
{
    FString Error;
    FVoxelFineMeshInput Input;
    Input.WorldEpoch = 1;
    Input.Incarnation = 7;
    Input.GeneratorIdentity = 11;
    Input.Shard = {FIntVector::ZeroValue, 32};
    FVoxelFineBuildState State;
    if (!TestTrue(TEXT("构建状态初始化"), State.Initialize(Input, Error)))
    {
        return false;
    }
    const FVoxelFineBuildAttempt Attempt = State.BeginAttempt();
    State.NotifyTopologyChanged();
    TestTrue(TEXT("分组合并不改变采样输入"), State.IsCurrent(Attempt));
    const auto Mesh = MakeShared<FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
    TestTrue(TEXT("合法空网格验证"), Mesh->ValidateAndCache());
    const auto Geometry = MakeShared<FVoxelFineShardGeometry, ESPMode::ThreadSafe>();
    Geometry->Attempt = Attempt;
    Geometry->Pieces.Add({Input.Shard, Mesh});
    Geometry->bKnownEmpty = true;
    Geometry->bCoverageValidated = true;
    const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
    TestTrue(TEXT("边界使用生产容量池"), Resources->Configure({}));
    TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BoundaryLease;
    const auto Consumer = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 7, Input.Shard.Min, 1);
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFineBoundarySnapshot>() +
        VoxelBoundedAllocation::ArrayBytes<FVoxelFineAtomBoundary>(8) +
        108 * VoxelBoundedAllocation::InitialGrowBytes<uint32>(1);
    if (!TestTrue(TEXT("完整边界分配前准入"), Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident, Bytes, BoundaryLease))) return false;
    auto Boundary = MakeShared<FVoxelFineBoundarySnapshot, ESPMode::ThreadSafe>();
    Boundary->Input = Input;
    Boundary->ResourceLease = MoveTemp(BoundaryLease);
    for (int32 Face = 0; Face < 6; ++Face) { Boundary->Owned[Face].Init(0, 1); Boundary->Halo[Face].Init(0, 1); }
    Boundary->Atoms.Reserve(8);
    for (int32 Z = 0; Z < 2; ++Z)
    for (int32 Y = 0; Y < 2; ++Y)
    for (int32 X = 0; X < 2; ++X)
    {
        auto& Atom = Boundary->Atoms.AddDefaulted_GetRef();
        Atom.Section = Input.Shard.Min / 16 + FIntVector(X, Y, Z);
        for (int32 Face = 0; Face < 6; ++Face) { Atom.Owned[Face].Init(0, 1); Atom.Halo[Face].Init(0, 1); }
    }
    Geometry->Boundary = MoveTemp(Boundary);
    TestEqual(TEXT("空网格仍是有效结果"), State.Complete(Attempt, false, Geometry, {}), EVoxelFineAttemptDisposition::Accepted);
    TestTrue(TEXT("空网格就绪有明确证明"), State.IsKnownEmpty());
    const TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> FrozenGeometry = State.GetPreparedGeometry();
    State.NotifyDataChanged(0, 1);
    TestFalse(TEXT("编辑使尚未封存的准备输入失效"), State.IsMeshReady());
    TestTrue(TEXT("已取得的不可变发布引用不因Desired变化消失"), FrozenGeometry.IsValid() && FrozenGeometry->Attempt == Attempt);
    const FVoxelFineBuildAttempt NewAttempt = State.BeginAttempt();
    TestEqual(TEXT("旧完成不能覆盖新Attempt"), State.Complete(Attempt, false, Geometry, {}), EVoxelFineAttemptDisposition::Stale);
    TestTrue(TEXT("新Attempt未被旧回调移除"), State.GetPendingAttempt().IsSet() && State.GetPendingAttempt().GetValue() == NewAttempt);
    TestFalse(TEXT("构建状态不把旧输入算成最新就绪"), State.IsCurrent(Attempt));
	const uint64 BeforePolicy = State.GetInput().MeshRevision;
	TestFalse(TEXT("完整内容身份未变不能生成新Mesh版本"), State.NotifyContentChanged(
		State.GetInput().GeneratorIdentity, State.GetInput().RegistryRevision, State.GetInput().ShapeRevision,
		State.GetInput().DataRevision, State.GetInput().OverlayRevision));
	TestEqual(TEXT("相同身份保留原版本"), State.GetInput().MeshRevision, BeforePolicy);
	const uint64 BeforeTopology = State.GetTopologyRevision();
	TestTrue(TEXT("生成器Registry和Shape同时变化是一个真实内容事件"), State.NotifyContentChanged(12, 21, 31, 0, 1));
	TestEqual(TEXT("一次完整身份变化只推进一个Mesh版本"), State.GetInput().MeshRevision, BeforePolicy + 1);
	TestEqual(TEXT("内容事件不混用拓扑版本"), State.GetTopologyRevision(), BeforeTopology);
	TestFalse(TEXT("内容策略变化拒绝旧Pending输入"), State.GetPendingAttempt().IsSet());
	TestTrue(TEXT("内容策略变化仍不破坏已冻结的实际发布几何引用"), FrozenGeometry.IsValid() && FrozenGeometry->Attempt == Attempt);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineOwnershipReceiptTest,
    "WHFramework.Voxel.FineRefactor.OwnershipReceipt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineOwnershipReceiptTest::RunTest(const FString& InParameters)
{
    FString Error;
    FVoxelFineHandoff Handoff(1);
    int32 SectionEvents = 0;
    bool bEventsAfterUnlock = true;
    Handoff.SetSectionChangedCallback([&](const FIntVector& Section)
    {
        ++SectionEvents;
        bEventsAfterUnlock &= !Handoff.IsLocked(Section);
    });
    const FIntVector FirstAtom(0, 0, 0);
    const FIntVector SecondAtom(1, 0, 0);
    const FVoxelFineOwner SharedShardOwner{EVoxelFineOwnerKind::F1Plus, FGuid::NewGuid(), 1};
    const auto Resource = MakeShared<FVoxelTaskCustomPayload, ESPMode::ThreadSafe>();
    FVoxelFineOwnerEvidence FirstVersion;
    FirstVersion.Owner = SharedShardOwner;
    FirstVersion.Content.WorldEpoch = 1;
    FirstVersion.Content.GeneratorIdentity = 11;
    FirstVersion.Content.OverlayRevision = 1;
    FirstVersion.Content.MeshRevision = 1;
    FirstVersion.Content.GeometryIdentity = FGuid::NewGuid();
    FirstVersion.Content.SampleStepCells = 1;
    FirstVersion.bGeometryReady = true;
    FirstVersion.bKnownEmpty = true;
    FirstVersion.bCanStage = true;
    FirstVersion.GeometryLease = Resource;

    bool bEvidenceCurrent = true;
    bool bCollisionCurrent = true;
    const auto IsEvidenceCurrent = [&bEvidenceCurrent](const FIntVector&, const FVoxelFineOwnerEvidence&)
    {
        return bEvidenceCurrent;
    };
    const auto IsCollisionCurrent = [&bCollisionCurrent](const FIntVector&, const FVoxelFineOwnerEvidence&)
    {
        return bCollisionCurrent;
    };
    const auto AreInterfacesCurrent = [](const FVoxelFineHandoffTransaction&, const FVoxelTaskCustomPayload&)
    {
        return true;
    };
    const auto MakeReceipt = [](const FVoxelFineFrozenHandoff& Frozen)
    {
        FVoxelFineRenderReceipt Receipt;
        Receipt.TransactionId = Frozen.Transaction.Id;
        Receipt.Stamp = Frozen.Stamp;
        for (const FVoxelFineOwnershipChange& Change : Frozen.Transaction.Changes)
        {
            FVoxelFineRenderReceiptAtom Atom;
            Atom.Section = Change.Section;
            Atom.Owner = Change.After.Owner;
            Atom.OwnerGeneration = Change.BeforeGeneration + 1;
            Atom.Content = Change.After.Content;
            Atom.bKnownEmpty = Change.After.bKnownEmpty;
            Receipt.Atoms.Add(MoveTemp(Atom));
        }
        return Receipt;
    };

    TArray<FVoxelFineOwnershipChange> InitialChanges;
    for (const FIntVector Atom : {FirstAtom, SecondAtom})
    {
        FVoxelFineOwnershipChange Change;
        Change.Section = Atom;
        Change.After = FirstVersion;
        Change.bRequiresCollision = true;
        InitialChanges.Add(MoveTemp(Change));
    }
    FVoxelFineHandoffTransaction Initial;
    if (!TestTrue(TEXT("初次呈现也经过正式交接事务"), Handoff.Prepare(InitialChanges, 1, Initial, Error)))
    {
        return false;
    }
    TestTrue(TEXT("生成完成不能建立初始Owner"), Handoff.FindPresentedAtom(FirstAtom) == nullptr);
    TArray<FVoxelFinePresentedAtom> Retirement;
    FVoxelFineRenderReceipt UnsealedReceipt;
    UnsealedReceipt.TransactionId = Initial.Id;
    UnsealedReceipt.Stamp = {1, 1, 100};
    TestEqual(TEXT("未封存不能接受RenderReceipt"), Handoff.CommitRenderReceipt(UnsealedReceipt, Retirement, Error), EVoxelFineReceiptDisposition::Invalid);
    TestEqual(TEXT("无效回执不能唤醒退休票据"), SectionEvents, 0);
    TSharedPtr<const FVoxelFineFrozenHandoff, ESPMode::ThreadSafe> InitialFrozen;
    bCollisionCurrent = false;
    TestFalse(TEXT("封存前缺真实碰撞证明则继续等待"), Handoff.SealTransaction(Initial.Id, {1, 1, 100}, Resource,
        IsEvidenceCurrent, IsCollisionCurrent, AreInterfacesCurrent, InitialFrozen, Error));
    bCollisionCurrent = true;
    if (!TestTrue(TEXT("封存初次提交"), Handoff.SealTransaction(Initial.Id, {1, 1, 100}, Resource,
        IsEvidenceCurrent, IsCollisionCurrent, AreInterfacesCurrent, InitialFrozen, Error)))
    {
        return false;
    }
    const FVoxelFineRenderReceipt InitialReceipt = MakeReceipt(*InitialFrozen);
    TestEqual(TEXT("完整模拟回执结算初始两个Atom"), Handoff.CommitRenderReceipt(InitialReceipt, Retirement, Error), EVoxelFineReceiptDisposition::Applied);
    TestEqual(TEXT("实际结算只唤醒事务中的两个Atom"), SectionEvents, 2);
    const FVoxelFinePresentedAtom* InitialFirst = Handoff.FindPresentedAtom(FirstAtom);
    if (!TestTrue(TEXT("Owner只在回执后建立"), InitialFirst != nullptr))
    {
        return false;
    }

    FVoxelFineOwnerEvidence SecondVersion = FirstVersion;
    SecondVersion.Content.OverlayRevision = 2;
    SecondVersion.Content.MeshRevision = 2;
    SecondVersion.Content.GeometryIdentity = FGuid::NewGuid();
    FVoxelFineOwnershipChange Update;
    Update.Section = FirstAtom;
    Update.Before = InitialFirst->Evidence.Owner;
    Update.BeforeGeneration = InitialFirst->OwnerGeneration;
    Update.After = SecondVersion;
    Update.bRequiresCollision = true;
    TArray<FVoxelFineOwnershipChange> UpdateChanges;
    UpdateChanges.Add(Update);
    FVoxelFineHandoffTransaction Replacement;
    TestTrue(TEXT("同Shard只替换一个Atom"), Handoff.Prepare(UpdateChanges, 2, Replacement, Error));
    TSharedPtr<const FVoxelFineFrozenHandoff, ESPMode::ThreadSafe> ReplacementFrozen;
    if (!TestTrue(TEXT("封存局部替换"), Handoff.SealTransaction(Replacement.Id, {1, 2, 101}, Resource,
        IsEvidenceCurrent, IsCollisionCurrent, AreInterfacesCurrent, ReplacementFrozen, Error)))
    {
        return false;
    }
    bEvidenceCurrent = false;
    bCollisionCurrent = false;
    TOptional<FVoxelFineHandoffTransaction> CanceledBeforeSeal;
    TestEqual(TEXT("封存后取消只能登记请求"), Handoff.RequestCancel(Replacement.Id, CanceledBeforeSeal), EVoxelFineCancelDisposition::RecordedAfterSeal);
    TestTrue(TEXT("封存后Cancel保持Atom锁和资源"), Handoff.IsLocked(FirstAtom) && Handoff.WasCancelRequested(Replacement.Id) && !CanceledBeforeSeal.IsSet());
    TestEqual(TEXT("封存后取消不能假装读者已退出"), SectionEvents, 2);
    const FVoxelFineRenderReceipt ReplacementReceipt = MakeReceipt(*ReplacementFrozen);
    FVoxelFineRenderReceipt WrongFrame = ReplacementReceipt;
    ++WrongFrame.Stamp.FrameCounter;
    TestEqual(TEXT("错误Frame回执不能结算"), Handoff.CommitRenderReceipt(WrongFrame, Retirement, Error), EVoxelFineReceiptDisposition::Invalid);
    TestEqual(TEXT("错Frame回执不产生退休唤醒"), SectionEvents, 2);
    TestEqual(TEXT("Desired与碰撞后来变化仍结算已经实际提交的历史帧"), Handoff.CommitRenderReceipt(ReplacementReceipt, Retirement, Error), EVoxelFineReceiptDisposition::Applied);
    TestEqual(TEXT("局部替换只产生一次局部唤醒"), SectionEvents, 3);
    const FVoxelFinePresentedAtom* PresentedFirst = Handoff.FindPresentedAtom(FirstAtom);
    const FVoxelFinePresentedAtom* PresentedSecond = Handoff.FindPresentedAtom(SecondAtom);
    if (!TestTrue(TEXT("逐Atom保存实际Presented"), PresentedFirst && PresentedSecond))
    {
        return false;
    }
    TestEqual(TEXT("替换Atom保持新版本"), PresentedFirst->Evidence.Content.OverlayRevision, uint64(2));
    TestEqual(TEXT("同Shard未替换Atom继续旧版本"), PresentedSecond->Evidence.Content.OverlayRevision, uint64(1));
    TestEqual(TEXT("旧几何引用转移退休队列而非立即释放"), Retirement.Num(), 1);
    TestTrue(TEXT("旧资源仍由退休记录保持"), Retirement[0].Evidence.GeometryLease.IsValid());
    TestFalse(TEXT("历史实际呈现不自动证明最新安全替代"), Handoff.CanReleaseF0(FirstAtom, false, false, false, false, false, IsEvidenceCurrent));
    bEvidenceCurrent = true;
    TestTrue(TEXT("最新替代有效且无其他需求允许F0数据退休"), Handoff.CanReleaseF0(FirstAtom, false, false, false, false, false, IsEvidenceCurrent));
    TestFalse(TEXT("未持久化仍阻止数据退休"), Handoff.CanReleaseF0(FirstAtom, false, false, false, false, true, IsEvidenceCurrent));
    TestEqual(TEXT("重复回执幂等"), Handoff.CommitRenderReceipt(ReplacementReceipt, Retirement, Error), EVoxelFineReceiptDisposition::Duplicate);
    TestEqual(TEXT("迟到旧回执不会回滚Owner"), Handoff.CommitRenderReceipt(InitialReceipt, Retirement, Error), EVoxelFineReceiptDisposition::Duplicate);
    TestEqual(TEXT("重复回执不重复退休同一引用"), Retirement.Num(), 1);
    TestEqual(TEXT("重复或迟到回执不重复唤醒"), SectionEvents, 3);
    Handoff.ForgetSettledReceipt(Initial.Id);
    TestEqual(TEXT("结算记录已清理的迟到回执无状态修改"), Handoff.CommitRenderReceipt(InitialReceipt, Retirement, Error), EVoxelFineReceiptDisposition::Unknown);
    TestEqual(TEXT("旧回执后仍是新Overlay版本"), Handoff.FindPresentedAtom(FirstAtom)->Evidence.Content.OverlayRevision, uint64(2));

    Update.BeforeGeneration = Handoff.FindPresentedAtom(FirstAtom)->OwnerGeneration;
    Update.After.Content.OverlayRevision = 3;
    Update.After.Content.MeshRevision = 3;
    Update.After.Content.GeometryIdentity = FGuid::NewGuid();
    UpdateChanges[0] = Update;
    FVoxelFineHandoffTransaction Canceled;
    TestTrue(TEXT("可建立下一次未封存事务"), Handoff.Prepare(UpdateChanges, 3, Canceled, Error));
    TestEqual(TEXT("封存前可取消并转移准备资源"), Handoff.RequestCancel(Canceled.Id, CanceledBeforeSeal), EVoxelFineCancelDisposition::CanceledBeforeSeal);
    TestTrue(TEXT("取消释放局部锁且资源交给调用方"), !Handoff.IsLocked(FirstAtom) && CanceledBeforeSeal.IsSet() && CanceledBeforeSeal->Changes[0].After.GeometryLease.IsValid());
    TestEqual(TEXT("封存前真实解锁唤醒原Owner等待"), SectionEvents, 4);
    TestTrue(TEXT("所有领域唤醒均发生在局部锁释放后"), bEventsAfterUnlock);

    TArray<FVoxelFineHandoffTransaction> PreparedDrain;
    TArray<TSharedPtr<const FVoxelFineFrozenHandoff, ESPMode::ThreadSafe>> FrozenDrain;
    TArray<FVoxelFinePresentedAtom> PresentedDrain;
    Handoff.DrainForShutdown(2, PreparedDrain, FrozenDrain, PresentedDrain);
    TestEqual(TEXT("世界结束转移两个实际呈现Atom给清理Owner"), PresentedDrain.Num(), 2);
    TestEqual(TEXT("旧世界回执不能写入新世界"), Handoff.CommitRenderReceipt(ReplacementReceipt, Retirement, Error), EVoxelFineReceiptDisposition::WorldExpired);
    TestEqual(TEXT("关闭世界撤销回调且不让旧回执唤醒新世界"), SectionEvents, 4);
    return true;
}

namespace VoxelFineRefactorTest
{
    class FAuthoredOverlay final : public IVoxelGenerationOverlay
    {
    public:
        virtual bool ApplySectionWindow(const FIntVector& InSectionCoordinate, const FVoxelSectionWindow& InWindow, TArrayView<FVoxelBlockState> InOutBlocks,
            FString& OutError, const TAtomic<bool>* InCancel) const override
        {
            if (InCancel && InCancel->Load())
            {
                OutError = TEXT("Canceled");
                return false;
            }
            if (InSectionCoordinate == FIntVector(0, 0, 2))
            {
                for (const FIntVector& Cell : {FIntVector(0, 0, 1), FIntVector(1, 0, 1)})
                {
                    if (InWindow.Contains(Cell)) InOutBlocks[InWindow.Index(Cell)] = FVoxelBlockState(9, 0);
                }
            }
            OutError.Reset();
            return true;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineExactSamplingTest,
    "WHFramework.Voxel.FineRefactor.ExactSamplingAndOverlay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineExactSamplingTest::RunTest(const FString& InParameters)
{
    FString Error;
    const auto Config = VoxelTest::MakeGenerationConfig();
    const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
    const auto Authored = MakeShared<VoxelFineRefactorTest::FAuthoredOverlay, ESPMode::ThreadSafe>();
    const auto Generator = MakeShared<FVoxelGenerationPipeline, ESPMode::ThreadSafe>(Config, Cache, Authored, false);
    auto Overlay = MakeShared<FVoxelOverlaySnapshotSet, ESPMode::ThreadSafe>();
    FVoxelOverlaySnapshot Erased;
    Erased.Section = FIntVector(0, 0, 2);
    Erased.Revision = 1;
    Erased.Blocks.Add(256, FVoxelBlockState());
    Overlay->Sections.Add(Erased.Section, Erased);
    FVoxelFineBuildAttempt Attempt;
    Attempt.Serial = 1;
    Attempt.Input.WorldEpoch = 1;
    Attempt.Input.Incarnation = 1;
    Attempt.Input.GeneratorIdentity = Config->Recipe->RecipeHash;
    Attempt.Input.OverlayRevision = 1;
    Attempt.Input.Shard = {FIntVector(0, 0, 32), 32};
    const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
    TestTrue(TEXT("采样使用生产容量池"), Resources->Configure({}));
    const FVoxelGenerationConsumerKey Consumer{EVoxelGenerationConsumerKind::FineShard, 1, FGuid::NewGuid(), 1,
        EVoxelGenerationSpatialKind::Bounds, Attempt.Input.Shard.Min, Attempt.Input.Shard.Bounds().Max, Attempt.Serial};
    FVoxelFineShardSampler Sampler(Generator, Overlay, Resources, Consumer);
    if (!TestTrue(TEXT("独立 Shard 采样初始化"), Sampler.Initialize(Attempt, Config->Recipe->RecipeHash,
        Config->Recipe->Settings.MinZ, Config->Recipe->Settings.MaxZ, Error)))
    {
        AddError(Error);
        return false;
    }
    TAtomic<bool> Cancel(false);
    EVoxelFineBuildProgress Progress = EVoxelFineBuildProgress::Working;
    int32 Microsteps = 0;
    while (Progress == EVoxelFineBuildProgress::Working && Microsteps++ < 1024)
    {
        Progress = Sampler.Advance(1, Cancel, Error);
    }
    if (!TestEqual(TEXT("按小步完成独立精确采样"), Progress, EVoxelFineBuildProgress::Complete))
    {
        AddError(Error);
        return false;
    }
    const TArray<FIntVector> RequiredSections = Sampler.GetRequiredSections();
    const auto Samples = Sampler.TakeResult();
    if (!TestTrue(TEXT("取得完整采样包"), Samples.IsValid()))
    {
        return false;
    }
    FVoxelBlockState State;
    TestTrue(TEXT("读取被删除的项目体素"), Samples->Snapshot.TrySample(FIntVector(0, 0, 1), State));
    TestTrue(TEXT("Persistent Air 覆盖项目生成结果"), State.IsAir());
    TestTrue(TEXT("读取未删除的项目体素"), Samples->Snapshot.TrySample(FIntVector(1, 0, 1), State));
    TestEqual(TEXT("保留项目生成 Overlay"), State.Pack(), FVoxelBlockState(9, 0).Pack());
    TMap<FIntVector, TArray<FVoxelBlockState>> Expected;
    for (const FIntVector& Key : RequiredSections)
    {
        FVoxelBlockState Generated[4096];
        if (!Generator->GenerateSection(Key, MakeArrayView(Generated), Error, &Cancel))
        {
            AddError(Error);
            return false;
        }
        TArray<FVoxelBlockState> Blocks;
        Blocks.Append(Generated, UE_ARRAY_COUNT(Generated));
        Expected.Add(Key, MoveTemp(Blocks));
    }
    bool bEqual = true;
    for (int32 LocalZ = -1; LocalZ <= 32 && bEqual; ++LocalZ)
    {
        for (int32 LocalY = -1; LocalY <= 32 && bEqual; ++LocalY)
        {
            for (int32 LocalX = -1; LocalX <= 32; ++LocalX)
            {
                const int32 Outside = int32(LocalX < 0 || LocalX >= 32) + int32(LocalY < 0 || LocalY >= 32) + int32(LocalZ < 0 || LocalZ >= 32);
                if (Outside > 1)
                {
                    continue;
                }
                const FIntVector Local(LocalX, LocalY, LocalZ);
                const FIntVector World = Attempt.Input.Shard.Min + Local;
                const FIntVector Section(int32(FVoxelFineSpatial::FloorDivide(World.X, 16)),
                    int32(FVoxelFineSpatial::FloorDivide(World.Y, 16)), int32(FVoxelFineSpatial::FloorDivide(World.Z, 16)));
                const FIntVector InSection = World - Section * 16;
                FVoxelBlockState Wanted = Expected.FindChecked(Section)[InSection.X + 16 * (InSection.Y + 16 * InSection.Z)];
                Overlay->ApplyAt(World, Wanted);
                FVoxelBlockState Actual;
                if (!Samples->Snapshot.TrySample(Local, Actual) || Actual != Wanted)
                {
                    bEqual = false;
                    break;
                }
            }
        }
    }
    TestTrue(TEXT("核心和六个面 Halo 与相同世界事实逐格一致"), bEqual);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineAdmissionRetentionTest,
    "WHFramework.Voxel.FineRefactor.AdmissionRetention",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineAdmissionRetentionTest::RunTest(const FString& InParameters)
{
    struct FExecuteBody
    {
        uint8 Data[2048]{};
        FVoxelTaskResult operator()(const TAtomic<bool>&) const
        {
            FVoxelTaskResult Result;
            Result.bSuccess = true;
            return Result;
        }
    };
    const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
    FVoxelViewManager::FFineShardSlot Slot;
    FVoxelFineMeshInput Input;
    Input.WorldEpoch = 37;
    Input.Incarnation = 3;
    Input.GeneratorIdentity = 59;
    Input.Shard = {FIntVector::ZeroValue, 32};
    Input.MeshRevision = 1;
    FString Error;
    if (!TestTrue(TEXT("原Fine槽建立完整内容身份"), Slot.Build.Initialize(Input, Error))) return false;
    const auto Attempt = Slot.Build.BeginAttempt();
    Slot.Consumer = {EVoxelGenerationConsumerKind::FineShard, 37, FGuid::NewGuid(), 3,
        EVoxelGenerationSpatialKind::Bounds, {}, {32, 32, 32}, Attempt.Serial};
    const auto FactResources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
    Slot.PendingCapture.Emplace();
    auto& Capture = Slot.PendingCapture.GetValue();
    Capture.Resources = FactResources;
    Capture.Consumer = Slot.Consumer;
    Capture.WorldEpoch = 37;
    Capture.Bounds = {{}, {32, 32, 32}};
    Capture.ReplicaDirectoryEpoch = FGuid::NewGuid();
    const auto OriginalDirectory = Capture.ReplicaDirectoryEpoch;
    if (!TestTrue(TEXT("目录切点元数据由原事实账本保活"), FactResources->Acquire(Slot.Consumer,
        EVoxelTerrainResourcePool::Input, VoxelBoundedAllocation::MapBytes<FIntVector, uint64>(1),
        Capture.Resident.ResourceLease))) return false;
    Capture.ContentRevisions.Reserve(1);
    Capture.ContentRevisions.Add({}, 7);
    const uint64 OriginalCaptureLease = Capture.Resident.ResourceLease->Serial();
    Slot.PendingRequest.Emplace();
    auto& Request = Slot.PendingRequest.GetValue();
    Request.Kind = EVoxelTaskKind::BuildFineShard;
    Request.GenerationConsumer = Slot.Consumer;
    Request.Stamp.WorldEpoch = 37;
    Request.Stamp.OwnerId = Slot.Consumer.OwnerId;
    Request.Stamp.OwnerIncarnation = 3;
    Request.Stamp.Token = Attempt.Serial;
    Request.Stamp.Revision = 1;
    Request.FirstRunnableSeconds = 17.5;
    if (!TestTrue(TEXT("真实owned callable先授额"), Request.BindExecute(Resources, Slot.Consumer, FExecuteBody(), Error))) return false;
    const auto OriginalStamp = Request.Stamp;
    const uint64 OriginalLease = Request.ExecuteBindingLease->Serial();
    auto Foreign = Slot.Consumer;
    Foreign.OwnerId = FGuid::NewGuid();
    TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ForeignLease;
    if (!TestTrue(TEXT("外部消费者保持实际输入占用"), Resources->Acquire(Foreign,
        EVoxelTerrainResourcePool::Input, 256, ForeignLease))) return false;
    FVoxelViewManager::WaitFineShardAdmission(Slot, Resources,
        Resources->ReleaseVersionExcludingOwner(Slot.Consumer));
    TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> OwnLease;
    if (!TestTrue(TEXT("原尝试临时工作占用"), Resources->Acquire(Slot.Consumer,
        EVoxelTerrainResourcePool::Working, 64, OwnLease))) return false;
    OwnLease.Reset();
    TestTrue(TEXT("自己的退款不能重试冻结Fine请求"), FVoxelViewManager::IsFineShardAdmissionWaiting(Slot));
    TestTrue(TEXT("等待保存完整Request"), Slot.PendingRequest.IsSet());
    TestTrue(TEXT("Worker接纳等待保留原固定目录切点"), Slot.PendingCapture.IsSet() &&
        Capture.ReplicaDirectoryEpoch == OriginalDirectory && Capture.ContentRevisions.FindRef({}) == 7);
    TestEqual(TEXT("原切点不释放后重新捕获"), Capture.Resident.ResourceLease->Serial(), OriginalCaptureLease);
    TestTrue(TEXT("等待保持原Stamp及attempt"), Request.Stamp == OriginalStamp && Slot.Build.BeginAttempt() == Attempt);
    TestEqual(TEXT("等待保持首次年龄"), Request.FirstRunnableSeconds, 17.5);
    TestEqual(TEXT("部分绑定保留同一个原生租约"), Request.ExecuteBindingLease->Serial(), OriginalLease);
    ForeignLease.Reset();
    TestFalse(TEXT("外部退款解除原Fine槽等待"), FVoxelViewManager::IsFineShardAdmissionWaiting(Slot));
    const TAtomic<bool> Cancel(false);
    TestTrue(TEXT("原冻结callable恢复后仍可消费"), Request.Execute(Cancel).bSuccess);
    TestFalse(TEXT("容量释放不制造Fine mesh Ready"), Slot.Build.IsMeshReady());
    FVoxelViewManager::ClearFineShardAdmission(Slot);
    TestFalse(TEXT("撤销清除部分绑定Request"), Slot.PendingRequest.IsSet());
    TestFalse(TEXT("撤销释放原固定目录切点"), Slot.PendingCapture.IsSet());
    TestFalse(TEXT("撤销解除原资源等待"), Slot.bWaitingAdmissionCapacity);
    TestEqual(TEXT("真实callable销毁后才归还原租约"), Resources->Snapshot().LeaseCount, 0);
    TestEqual(TEXT("切点退出后原事实账本归零"), FactResources->Snapshot().LeaseCount, 0);
    return true;
}

#endif
