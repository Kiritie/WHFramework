#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Streaming/VoxelInterestManager.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainWorkSelection.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelInterestPriorityPublicationTest,
	"WHFramework.Voxel.FineRefactor.Interest.PriorityPublication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelInterestPriorityPublicationTest::RunTest(const FString& InParameters)
{
	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::FineVisual;
	Source.Priority = 4;
	Source.SchedulingPriority = INDEX_NONE;
	FVoxelInterestSet Prepared;
	auto& Contribution = Prepared.Sources.Add(Source.Id);
	Contribution.Source = Source;
	Contribution.DataSections.Add(FIntVector::ZeroValue);
	Contribution.FineDataSections.Add(FIntVector::ZeroValue);
	Contribution.F0CoreSections.Add(FIntVector::ZeroValue);
	auto& Demand = Prepared.Exact.Add(FIntVector::ZeroValue);
	Demand.bData = Demand.bExact = Demand.bFineRender = Demand.bMovementCriticalFine = true;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = 0; Manifest.Settings.MaxZ = 16;
	FVoxelViewSettings Settings;
	FVoxelInterestSet Empty;
	FVoxelInterestManager BaselineManager;
	auto Previous = BaselineManager.Finalize(MakeArrayView(&Source, 1), Manifest, Settings, MoveTemp(Prepared), &Empty, false);
	if (!TestTrue(TEXT("基准范围完成"), Previous.PlanningError.IsEmpty())) return false;
	Previous.PublicationRevision = 21;
	const auto PreviousInputs = Previous.PublicationInputs;
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	Resources->Configure({});
	const auto Consumer = FVoxelGenerationConsumerKey::Section(31, FGuid::NewGuid(), 1, {}, 1);
	FVoxelInterestManager Manager;
	Manager.BindResources(Resources, Consumer);
	FVoxelInterestSet Next;
	FString Error;
	if (!TestTrue(TEXT("复制前授予独立容量"), Manager.CopyForPriority(Previous, Next, Error))) return false;
	Source.Priority = 9;
	Next.Sources.FindChecked(Source.Id).Source = Source;
	if (!TestTrue(TEXT("只更新优先级"), FVoxelInterestManager::RefreshPriorities(
		MakeArrayView(&Source, 1), 32, Next, Error, Resources, Consumer))) return false;
	if (!TestTrue(TEXT("发布输入冻结新优先级"), Manager.BuildPublicationInputs(Next, Error))) return false;
	TestEqual(TEXT("内容发布版本不变"), Next.PublicationRevision, uint64(21));
	TestEqual(TEXT("旧发布输入仍保持原优先级"), PreviousInputs->Admissions[0].Priority, 4);
	TestEqual(TEXT("新发布输入包含新优先级"), Next.PublicationInputs->Admissions[0].Priority, 9);
	TestEqual(TEXT("精确集合保持"), Next.Exact.Num(), Previous.Exact.Num());
	TestTrue(TEXT("复制表的独立租约涵盖实际分配"), Next.CopyLease && Next.CopyLease->Bytes() >= Next.GetOwnedAllocatedBytes());
	TestTrue(TEXT("不可变Fine内容集合复用"), Next.FineSections == Previous.FineSections);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineLayoutCapacityContinuationTest,
	"WHFramework.Voxel.FineRefactor.Interest.LayoutCapacityContinuation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineLayoutCapacityContinuationTest::RunTest(const FString& InParameters)
{
	FVoxelFinePageLayout Baseline;
	FString Error;
	if (!Baseline.Initialize(3, 32, 64, Error)) return false;
	TVoxelFineSetDelta<FVoxelFineShardKey> Initial;
	for (int32 Index = 0; Index < 8; ++Index)
		Initial.Added.Add({FIntVector(Index & 1, (Index >> 1) & 1, (Index >> 2) & 1) * 32, 32});
	if (!Baseline.ApplyResidencyDelta(Initial, Error)) return false;
	FVoxelFinePageLayoutDelta LayoutDelta;
	Baseline.Advance(64, LayoutDelta);
	if (!TestEqual(TEXT("完整兄弟合并为一个Page"), Baseline.GetPages().Num(), 1)) return false;
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	Resources->Configure({});
	const auto Consumer = FVoxelGenerationConsumerKey::Section(31, FGuid::NewGuid(), 1, {}, 1);
	auto Candidate = Baseline.Capture(Resources, Consumer, Error);
	if (!Candidate) return false;
	TVoxelFineSetDelta<FVoxelFineShardKey> Removed;
	Removed.Removed.Add({FIntVector::ZeroValue, 32});
	if (!Candidate->ApplyResidencyDelta(Removed, Error)) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	const auto State = Resources->Snapshot();
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
		State.Limits.WorkingBytes - State.Used[int32(EVoxelTerrainResourcePool::Working)], Blocker)) return false;
	TestEqual(TEXT("容量不足不消费脏根"), Candidate->Advance(1, LayoutDelta, &Error, false), 0);
	TestTrue(TEXT("等待原因明确"), Error.StartsWith(TEXT("CapacityBlocked")));
	TestTrue(TEXT("同一脏根保留续行"), Candidate->IsPending());
	TestEqual(TEXT("旧Page仍服务"), Candidate->GetPages().Num(), 1);
	Blocker.Reset();
	TestEqual(TEXT("释放后同一脏根完成"), Candidate->Advance(1, LayoutDelta, &Error, false), 1);
	TestFalse(TEXT("没有残留脏根"), Candidate->IsPending());
	TestEqual(TEXT("缺失兄弟不当空Shard合并"), Candidate->GetPages().Num(), 7);
	TestEqual(TEXT("捕获前的根保持独立"), Baseline.GetPages().Num(), 1);
	TestTrue(TEXT("生产模式报告实际拓扑变化"), LayoutDelta.bChanged);
	TestTrue(TEXT("生产模式不复制Page正文到Delta"), LayoutDelta.Added.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineLeaseCapacityIsolationTest,
	"WHFramework.Voxel.FineRefactor.Interest.LeaseCapacityIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineLeaseCapacityIsolationTest::RunTest(const FString& InParameters)
{
	FVoxelFineResidencyLeases Baseline;
	const auto FirstSource = FGuid::NewGuid();
	const auto SecondSource = FGuid::NewGuid();
	TSet<FVoxelFineShardKey> Shards;
	const FVoxelFineShardKey Key{FIntVector::ZeroValue, 32};
	Shards.Add(Key);
	Baseline.SetSource(FirstSource, Shards);
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	Resources->Configure({});
	const auto Consumer = FVoxelGenerationConsumerKey::Section(31, FGuid::NewGuid(), 1, {}, 1);
	FString Error;
	auto Candidate = Baseline.Capture(Resources, Consumer, Error);
	if (!Candidate) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	const auto State = Resources->Snapshot();
	const uint64 Available = State.Limits.InputBytes - State.Used[int32(EVoxelTerrainResourcePool::Input)] -
		VoxelBoundedAllocation::SharedBytes<FVoxelTerrainResourceLease>();
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Available, Blocker)) return false;
	TVoxelFineSetDelta<FVoxelFineShardKey> Delta;
	TestFalse(TEXT("新Source容量不足时不改变租约"), Candidate->TrySetSource(SecondSource, Shards, Delta, Error));
	TestEqual(TEXT("原Source引用保持"), Candidate->ReferenceCount(Key), 1);
	Blocker.Reset();
	if (!TestTrue(TEXT("容量恢复后接纳第二Source"), Candidate->TrySetSource(SecondSource, Shards, Delta, Error))) return false;
	TestEqual(TEXT("共享Shard只增引用"), Candidate->ReferenceCount(Key), 2);
	TestTrue(TEXT("共享Shard不产生新增内容"), Delta.Added.IsEmpty());
	if (!Candidate->TryRemoveSource(FirstSource, Delta, Error)) return false;
	TestEqual(TEXT("撤销一个Source不撤销另一Source"), Candidate->ReferenceCount(Key), 1);
	TestEqual(TEXT("旧捕获不受新代修改"), Baseline.ReferenceCount(Key), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFinePendingReverseCapacityTest,
	"WHFramework.Voxel.FineRefactor.Interest.PendingReverseCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFinePendingReverseCapacityTest::RunTest(const FString& InParameters)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	Resources->Configure({});
	const auto Consumer = FVoxelGenerationConsumerKey::Section(31, FGuid::NewGuid(), 1, {}, 1);
	FVoxelFineResidency Residency;
	Residency.BindResources(Resources.ToSharedRef(), Consumer);
	FString Error;
	if (!Residency.Initialize(FGuid::NewGuid(), {}, {}, 32, 64, 80, -512, 512, Error)) return false;
	const uint64 CurrentSerial = Residency.GetCurrent().Serial;
	FVoxelFineMovementDelta Movement;
	FVoxelFineMigrationDecision Decision;
	if (!Residency.UpdateMovement(FIntVector(144, 0, 0), FVector::ZeroVector, 0.0, Movement, Error, false) ||
		!Residency.PrepareWindow(FIntVector(144, 0, 0), FVector::ZeroVector, 0.5, Decision, Error, false)) return false;
	if (!TestTrue(TEXT("首次预热产生邻窗"), Residency.GetPending().IsSet())) return false;
	const uint64 FirstPending = Residency.GetPending()->Serial;
	const FVector ReverseVelocity(-32.0, 0.0, 0.0);
	if (!Residency.UpdateMovement(FIntVector(128, 0, 0), ReverseVelocity, 0.0, Movement, Error, false) ||
		!Residency.PrepareWindow(FIntVector(128, 0, 0), ReverseVelocity, 0.5, Decision, Error, false)) return false;
	TestEqual(TEXT("小幅折返继续使用可服务Pending"), Residency.GetPending()->Serial, FirstPending);
	if (!Residency.UpdateMovement(FIntVector(-144, 0, 0), ReverseVelocity, 0.0, Movement, Error, false)) return false;
	const auto State = Resources->Snapshot();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	const uint64 Available = State.Limits.InputBytes - State.Used[int32(EVoxelTerrainResourcePool::Input)] -
		VoxelBoundedAllocation::SharedBytes<FVoxelTerrainResourceLease>();
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Available, Blocker)) return false;
	TestFalse(TEXT("容量不足不取消正在预热的窗口"), Residency.PrepareWindow(
		FIntVector(-144, 0, 0), ReverseVelocity, 0.5, Decision, Error));
	TestEqual(TEXT("等待期间保留相同Pending身份"), Residency.GetPending()->Serial, FirstPending);
	TestEqual(TEXT("等待期间Current持续有效"), Residency.GetCurrent().Serial, CurrentSerial);
	Blocker.Reset();
	if (!Residency.PrepareWindow(FIntVector(-144, 0, 0), ReverseVelocity, 0.5, Decision, Error)) return false;
	TestTrue(TEXT("远端折返替换不可服务的Pending"), Residency.GetPending()->Serial > FirstPending);
	TestTrue(TEXT("新窗口向负方向预热"), Residency.GetPending()->Anchor.X < Residency.GetCurrent().Anchor.X);
	TestEqual(TEXT("替换Pending不能切换Current"), Residency.GetCurrent().Serial, CurrentSerial);
	for (const auto& Key : Residency.GetCurrent().Shards)
	{
		if (!TestTrue(TEXT("替换仅撤销Pending独有需求"), Residency.Retains(Key) && !Decision.Residency.Removed.Contains(Key))) return false;
	}
	TestTrue(TEXT("替换回执包含撤销的旧Pending尾部"), !Decision.Residency.Removed.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainSelectionCapacityTest,
	"WHFramework.Voxel.FineRefactor.Interest.SelectionCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainSelectionCapacityTest::RunTest(const FString& InParameters)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	Resources->Configure({});
	const auto Consumer = FVoxelGenerationConsumerKey::Section(31, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Bootstrap;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, 1, Bootstrap)) return false;
	Bootstrap.Reset();
	const auto State = Resources->Snapshot();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	const uint64 Available = State.Limits.InputBytes - State.Used[int32(EVoxelTerrainResourcePool::Input)] -
		VoxelBoundedAllocation::SharedBytes<FVoxelTerrainResourceLease>();
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Available, Blocker)) return false;
	{
		FVoxelTerrainWorkSelection Selection;
		Selection.BindResources(Resources.ToSharedRef(), Consumer);
		FVoxelTerrainServiceCandidate Candidate;
		Candidate.Index = 3;
		Candidate.Priority = {EVoxelTerrainServiceBand::P2, 1.0, 0.0, 0, FGuid::NewGuid()};
		Candidate.FirstQueuedSeconds = 1.0;
		Candidate.Sequence = 1;
		Candidate.EstimatedMicroseconds = 250.0;
		Candidate.bRunnable = true;
		FVoxelTerrainServiceCharge Charge;
		TestEqual(TEXT("选择账本分配前等待容量"), Selection.Select(MakeArrayView(&Candidate, 1), {}, 2.0, Charge), INDEX_NONE);
		TestEqual(TEXT("容量不足不能发行收费身份"), Charge.Serial, uint64(0));
		TestFalse(TEXT("容量等待不污染公平账本"), Selection.HasAccountingFault());
		Blocker.Reset();
		if (!TestEqual(TEXT("释放后同一候选可接纳"), Selection.Select(MakeArrayView(&Candidate, 1), {}, 2.0, Charge), 3)) return false;
		TestTrue(TEXT("首次实际收费没有被失败尝试跳过"), Charge.Serial == 1);
		TestTrue(TEXT("正式账本归还未开始工作"), Selection.RefundBeforeStart(Charge.Serial));
		TestTrue(TEXT("已结算可重置"), Selection.Reset());
	}
	TestEqual(TEXT("所有者析构后元数据租约归还"), Resources->Snapshot().LeaseCount, 0);
	return true;
}

#endif
