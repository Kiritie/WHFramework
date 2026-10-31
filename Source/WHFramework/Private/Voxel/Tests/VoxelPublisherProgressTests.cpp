#include "Voxel/Rendering/VoxelViewPublisher.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Voxel/VoxelModule.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPublisherProgressAndNearOrderTest,
	"WHFramework.Voxel.Rendering.PublisherProgressAndNearOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPublisherProgressAndNearOrderTest::RunTest(const FString& Parameters)
{
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler(true);
	if (!TestTrue(TEXT("发布选择夹具初始化真实账本"), Scheduler.InitializeStatistics(1))) return false;
	uint64 GateStateRevision = 0;
	FVoxelViewPublisher Publisher(*Module, Scheduler, 1);
	FVoxelPublishBudget Budget;
	Budget.Begin(FPlatformTime::Seconds(), 100.0, 64);
	int32 NoOpCalls = 0;
	TestFalse(TEXT("实际无进度的阶段返回 false"), Publisher.RunBudgetedPhase(Budget,
		EVoxelPublishPhase::Admission, [&NoOpCalls] { ++NoOpCalls; return false; }));
	TestEqual(TEXT("空操作只执行一次"), NoOpCalls, 1);
	TestEqual(TEXT("空操作仍如实记录测量次数"), Budget.GetTiming(EVoxelPublishPhase::Admission).Operations, 1);
	TestTrue(TEXT("空操作后共享预算仍可运行其他阶段"),
		Budget.TryStart(EVoxelPublishPhase::Commit, FPlatformTime::Seconds(), 0.01));

	const auto Publication = MakeShared<FVoxelViewPublisher::FPublication>();
	Publication->Serial = 2;
	Publication->bCoveragePrepared = true;
	Publication->bGroupsPrepared = true;
	Publisher.Publications.Add(Publication);
	Publisher.bPrioritiesDirty = false;
	const auto AddReady = [&](const double Distance, const EVoxelTerrainServiceBand Band)
	{
		const int32 Index = Publication->Groups.AddDefaulted();
		auto& Group = Publication->Groups[Index];
		Group.Keys.Add({0, FIntVector(Index, 0, 0), 0});
		Group.bPriorityReady = true;
		Group.Priority.Service.Band = Band;
		Group.Priority.Service.DistanceCells = Distance;
		Group.Priority.Service.SourcePriority = 0;
		Publisher.QueueReadyGroup(Publication, Index);
		return Index;
	};
	for (int32 Index = 0; Index < 257; ++Index) AddReady(1000.0 + Index, EVoxelTerrainServiceBand::P2);
	const int32 NearIndex = AddReady(1.0, EVoxelTerrainServiceBand::P6);
	const auto Select = [&]()
	{
		FVoxelTerrainServiceCharge Charge;
		const double Started = FPlatformTime::Seconds();
		const int32 ReadyIndex = Publisher.SelectReadyGroup(Publisher.ReadyCommitGroups,
			Publisher.CommitSelection, Charge, true);
		Publisher.SettleReadyGroup(Publisher.CommitSelection, Charge, Started, true);
		return ReadyIndex == INDEX_NONE ? INDEX_NONE : Publisher.ReadyCommitGroups[ReadyIndex].GroupIndex;
	};
	TestEqual(TEXT("257 个远组之后加入的最近组仍首先提交，后台服务带不反转距离"), Select(), NearIndex);
	const int32 SafetyIndex = AddReady(2000.0, EVoxelTerrainServiceBand::P0);
	TestEqual(TEXT("原 P0 安全职责优先级保持"), Select(), SafetyIndex);
	Publisher.SetCommitGate([](const FVoxelPublishGroupKey&) { return false; }, GateStateRevision);
	Budget.Begin(FPlatformTime::Seconds(), 100.0, 64);
	Publisher.Tick(Budget);
	TestTrue(TEXT("全部门禁阻塞时 Commit 仅扫描一次而不耗尽 64 次操作"),
		Budget.GetTiming(EVoxelPublishPhase::Commit).Operations <= 1);
	TestEqual(TEXT("准备与接纳已完成时不反复空跑 Admission"),
		Budget.GetTiming(EVoxelPublishPhase::Admission).Operations, 0);
	TestTrue(TEXT("阻塞后仍给其他主线程阶段保留共享预算"),
		Budget.TryStart(EVoxelPublishPhase::Plan, FPlatformTime::Seconds(), 0.01));
	TestFalse(TEXT("门禁未通过不能伪造提交"), Publication->Groups[NearIndex].bCommitted);

	bool bAllowNear = false;
	Publisher.SetCommitGate([&](const FVoxelPublishGroupKey& Key)
	{
		return Key.Coordinate.X != SafetyIndex && (Key.Coordinate.X != NearIndex || bAllowNear);
	}, GateStateRevision);
	TestEqual(TEXT("真实门禁阻塞的近组不冻结独立可运行组"), Select(), 0);
	bAllowNear = true;
	++GateStateRevision;
	Publisher.InvalidateCommitReadiness();
	TestEqual(TEXT("覆盖门禁改变后重新检查最近组"), Select(), NearIndex);
	const auto Older = MakeShared<FVoxelViewPublisher::FPublication>();
	Older->Serial = 1;
	for (int32 Index = 0; Index < 96; ++Index)
	{
		auto& Group = Older->Groups.AddDefaulted_GetRef();
		Group.Bounds = FBox(FVector(1000000.0 + Index * 64.0, 0, 0), FVector(1000032.0 + Index * 64.0, 32, 32));
	}
	Publication->Groups[NearIndex].Bounds = FBox(FVector::ZeroVector, FVector(32));
	Publisher.Publications.Add(Older);
	Publisher.InvalidateCommitReadiness();
	TestEqual(TEXT("近组依赖索引尚需续算，不能把扫描切片误判为可让远组超车的真实阻塞"), Select(), INDEX_NONE);
	int32 AfterContinuation = INDEX_NONE;
	for (int32 Slice = 0; Slice < 32 && AfterContinuation == INDEX_NONE; ++Slice) AfterContinuation = Select();
	TestEqual(TEXT("依赖证明完成后保留最近组为可见前沿"), AfterContinuation, NearIndex);
	Publisher.InvalidateCommitReadiness();
	const int32 CursorBefore = Publisher.ReadySelectionCursors[1];
	FVoxelTerrainServiceCharge ExpiredCharge;
	TestEqual(TEXT("已过截止时间不继续候选扫描"),
		Publisher.SelectReadyGroup(Publisher.ReadyCommitGroups, Publisher.CommitSelection,
			ExpiredCharge, true, FPlatformTime::Seconds() - 1.0), INDEX_NONE);
	TestEqual(TEXT("截止时间不会把近序游标推向远处"), Publisher.ReadySelectionCursors[1], CursorBefore);
	return true;
}
#endif
