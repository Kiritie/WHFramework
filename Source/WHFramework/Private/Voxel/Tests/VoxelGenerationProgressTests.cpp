#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Voxel/Rendering/VoxelTerrainFrameBudget.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Task/VoxelTerrainWorkSelection.h"
#include "../Rendering/VoxelCoarseBuildStorage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCoarseCapacityPredicateTest,
	"WHFramework.Voxel.FineRefactor.Generation.CoarseCapacityPredicate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCoarseCapacityPredicateTest::RunTest(const FString& InParameters)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	FVoxelTerrainResourceLimits Limits;
	Limits.StagedBytes = 4096;
	Limits.WorkingBytes = 4096;
	if (!TestTrue(TEXT("配置原账本"), Resources->Configure(Limits)))
	{
		return false;
	}
	const auto Consumer = FVoxelGenerationConsumerKey::Section(37, FGuid::NewGuid(), 1, {}, 1);
	const auto OtherConsumer = FVoxelGenerationConsumerKey::Section(37, FGuid::NewGuid(), 1, {}, 2);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Metadata;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResultInput;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> StagedBlocker;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, 128, Metadata) ||
		!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, 128, ResultInput) ||
		!Resources->Acquire(OtherConsumer, EVoxelTerrainResourcePool::Staged, Limits.StagedBytes, StagedBlocker))
	{
		return false;
	}
	FVoxelCoarseBuildStorage Storage(Resources, Consumer, Metadata, ResultInput);
	Storage.GeometryCapacityBytes = Limits.StagedBytes;
	Storage.WorkingCapacityBytes = 128;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
	FVoxelTaskResult Result;
	TestFalse(TEXT("Staged 满时原任务暂停"), Storage.Begin(Result, Working));
	TestTrue(TEXT("暂停保留原续跑身份"), Result.bSuccess && Result.bContinue);
	TestEqual(TEXT("等待目标是 Staged"), Storage.WaitingPool, EVoxelTerrainResourcePool::Staged);
	TestEqual(TEXT("等待保留完整所需字节"), Storage.WaitingBytes, uint64(4096));
	for (int32 Iteration = 0; Iteration < 8; ++Iteration)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Temporary;
		if (!Resources->Acquire(OtherConsumer, EVoxelTerrainResourcePool::Working, 64, Temporary))
		{
			return false;
		}
		Temporary.Reset();
		TestEqual(TEXT("无关 Working 释放不使 Staged 任务可运行"), Storage.BlockReason(), EVoxelTaskBlockReason::Memory);
	}
	TestTrue(TEXT("同池少量释放成功"), StagedBlocker->Resize(2048));
	TestEqual(TEXT("同池字节仍不足时保持等待"), Storage.BlockReason(), EVoxelTaskBlockReason::Memory);
	const auto BeforeQuery = Resources->Snapshot();
	for (int32 Iteration = 0; Iteration < 8; ++Iteration)
	{
		Storage.BlockReason();
	}
	const auto AfterQuery = Resources->Snapshot();
	TestEqual(TEXT("查询不授予句柄"), AfterQuery.LeaseCount, BeforeQuery.LeaseCount);
	TestEqual(TEXT("查询不扩容账本"), AfterQuery.LedgerMetadataBytes, BeforeQuery.LedgerMetadataBytes);
	TestEqual(TEXT("查询不制造释放或授额事件"), AfterQuery.Version, BeforeQuery.Version);
	StagedBlocker.Reset();
	TestEqual(TEXT("完整容量归还后原任务可运行"), Storage.BlockReason(), EVoxelTaskBlockReason::None);
	Result = FVoxelTaskResult();
	TestTrue(TEXT("查询后仍通过原正式授额完成接纳"), Storage.Begin(Result, Working));
	TestTrue(TEXT("完成正文与微步缓冲均有真实租约"), Storage.GeometryLease.IsValid() && Working.IsValid());
	TestEqual(TEXT("完成正文仍在原 Staged 账本"), Storage.GeometryLease->GetPool(), EVoxelTerrainResourcePool::Staged);
	TestTrue(TEXT("消费者身份不因续跑改变"), Storage.Consumer == Consumer);
	EVoxelTerrainResourceAdmission Admission;
	TestTrue(TEXT("无效身份查询可解释"), Resources->TryQueryAcquire({}, EVoxelTerrainResourcePool::Working, 1, Admission));
	TestEqual(TEXT("无效身份不能获准执行"), Admission, EVoxelTerrainResourceAdmission::InvalidRequest);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelContinuationQuotaTest,
	"WHFramework.Voxel.FineRefactor.Generation.ContinuationOutsideApplyQuota",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelContinuationQuotaTest::RunTest(const FString& InParameters)
{
	TAtomic<int32> FirstSteps{0};
	TAtomic<int32> SecondSteps{0};
	FVoxelTaskScheduler Scheduler(true);
	FVoxelTaskBudget Budget;
	Budget.MaxConcurrentTasks = 2;
	Budget.MaxCompletedResultsPerFrame = 1;
	Budget.MaxHeavyCompletedResultsPerFrame = 1;
	Scheduler.SetBudget(Budget);
	const FGuid Owner = FGuid::NewGuid();
	const auto Enqueue = [&](TAtomic<int32>& Steps, const uint64 Token)
	{
		FVoxelTaskRequest Request;
		Request.Kind = EVoxelTaskKind::BuildFineShard;
		Request.Stamp.OwnerId = Owner;
		Request.Stamp.WorldEpoch = 37;
		Request.Stamp.Token = Token;
		Request.WorkClass = EVoxelWorkClass::Visible;
		Request.ReservedBytes = 1024;
		Request.bResumable = true;
		Request.Execute = [&Steps](const TAtomic<bool>& Cancel)
		{
			FVoxelTaskResult Result;
			Result.bCanceled = Cancel.Load();
			if (!Result.bCanceled)
			{
				const int32 Step = Steps.Load() + 1;
				Steps.Store(Step);
				Result.bSuccess = true;
				Result.bContinue = Step < 3;
			}
			return Result;
		};
		return Scheduler.Enqueue(MoveTemp(Request));
	};
	{
		FVoxelTaskScheduler::FEnqueueBatch OuterBatch(Scheduler);
		TestTrue(TEXT("第一条原任务正式入队"), Enqueue(FirstSteps, 1));
		{
			FVoxelTaskScheduler::FEnqueueBatch InnerBatch(Scheduler);
			TestTrue(TEXT("嵌套批次的第二条原任务正式入队"), Enqueue(SecondSteps, 2));
			InnerBatch.Finish();
			InnerBatch.Finish();
		}
		TestEqual(TEXT("内层退出不提前启动 worker"), Scheduler.GetDiagnostics().Running, 0);
		TestEqual(TEXT("批次保留两个已接纳 Pending"), Scheduler.GetDiagnostics().Pending, 2);
	}
	const double FirstDeadline = FPlatformTime::Seconds() + 2.0;
	while (Scheduler.GetDiagnostics().CompletedAwaitingApply < 2 && FPlatformTime::Seconds() < FirstDeadline)
	{
		FPlatformProcess::SleepNoStats(0.001f);
	}
	if (!TestEqual(TEXT("两个真实原生切片均已结束"), Scheduler.GetDiagnostics().CompletedAwaitingApply, 2))
	{
		Scheduler.StopAndJoin();
		return false;
	}
	int32 Applied = 0;
	const auto Apply = [&Applied](FVoxelTaskResult&& Result)
	{
		if (Result.bSuccess && !Result.bContinue)
		{
			++Applied;
		}
	};
	Scheduler.Tick(Apply, 20.0);
	TestEqual(TEXT("每帧1个结果配额仍允许回收2个轻量续跑切片"), Scheduler.GetDiagnostics().ContinuationSlicesRetired, uint64(2));
	TestEqual(TEXT("续跑不伪造最终 Apply"), Applied, 0);
	const double CompletionDeadline = FPlatformTime::Seconds() + 2.0;
	while (Applied < 2 && FPlatformTime::Seconds() < CompletionDeadline)
	{
		Scheduler.Tick(Apply, 20.0);
		FPlatformProcess::SleepNoStats(0.001f);
	}
	TestEqual(TEXT("两条原任务都能终结"), Applied, 2);
	TestEqual(TEXT("第一条原任务没有重建其进度"), FirstSteps.Load(), 3);
	TestEqual(TEXT("第二条原任务没有重建其进度"), SecondSteps.Load(), 3);
	TestEqual(TEXT("只有最终结果记入 completed"), Scheduler.GetDiagnostics().ByKind.FindRef(EVoxelTaskKind::BuildFineShard).Completed, uint64(2));
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTwoWorkerServiceTest,
	"WHFramework.Voxel.FineRefactor.Generation.TwoWorkerServiceProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTwoWorkerServiceTest::RunTest(const FString& InParameters)
{
	FVoxelTerrainWorkSelection Selection;
	FVoxelTerrainServiceCandidate Candidates[2];
	Candidates[0].Index = 0;
	Candidates[0].Priority.Band = EVoxelTerrainServiceBand::P0;
	Candidates[0].Priority.DistanceCells = 0.0;
	Candidates[0].EstimatedMicroseconds = 250.0;
	Candidates[0].Sequence = 1;
	Candidates[0].bRunnable = true;
	Candidates[1].Index = 1;
	Candidates[1].Priority.Band = EVoxelTerrainServiceBand::P5;
	Candidates[1].Priority.DistanceCells = 16000.0;
	Candidates[1].EstimatedMicroseconds = 20000.0;
	Candidates[1].Sequence = 2;
	Candidates[1].bRunnable = true;
	FVoxelTerrainServiceCapacity Capacity;
	Capacity.MaximumWorkers = 2;
	Capacity.RunningWorkers = 1;
	Capacity.NearReservedWorkers = 1;
	Capacity.MinimumNonNearWorkers = 1;
	for (int32 Iteration = 0; Iteration < 16; ++Iteration)
	{
		FVoxelTerrainServiceCharge Charge;
		Capacity.RunningNearWorkers = 0;
		TestEqual(TEXT("远任务占据一个槽时，空出的另一槽服务近处"),
			Selection.Select(MakeArrayView(Candidates), Capacity, 1.0 + Iteration, Charge), 0);
		TestTrue(TEXT("近处按真实短耗时结算"), Selection.Settle(Charge.Serial, 250.0));
		Capacity.RunningNearWorkers = 1;
		TestEqual(TEXT("近任务占据一个槽时，远任务仍有实际服务"),
			Selection.Select(MakeArrayView(Candidates), Capacity, 1.0 + Iteration, Charge), 1);
		TestTrue(TEXT("远处按真实长耗时结算"), Selection.Settle(Charge.Serial, 20000.0));
	}
	TestFalse(TEXT("异构耗时服务没有破坏欠额账本"), Selection.HasAccountingFault());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelIdleKindBorrowTest,
	"WHFramework.Voxel.FineRefactor.Generation.IdleKindBorrow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelIdleKindBorrowTest::RunTest(const FString& InParameters)
{
	for (const bool bAutomatic : {false, true})
	{
		FVoxelTaskScheduler Scheduler(true);
		FVoxelTaskBudget Budget;
		Budget.MaxConcurrentTasks = 2;
		Budget.MaxConcurrentSurfaceTasks = 1;
		Budget.MaxConcurrentCoarseTerrainTasks = 1;
		Budget.bBorrowIdleKindSlots = bAutomatic;
		Scheduler.SetBudget(Budget);
		const FGuid Owner = FGuid::NewGuid();
		TAtomic<bool> ReleaseWorkers(false);
		{
			FVoxelTaskScheduler::FEnqueueBatch Batch(Scheduler);
			for (uint64 Token = 1; Token <= 3; ++Token)
			{
				FVoxelTaskRequest Request;
				Request.Kind = EVoxelTaskKind::BuildSurface;
				Request.Stamp.OwnerId = Owner;
				Request.Stamp.WorldEpoch = 37;
				Request.Stamp.Token = Token;
				Request.WorkClass = EVoxelWorkClass::Visible;
				Request.ReservedBytes = 1024;
				Request.GetBlockReason = [Token]
				{
					return Token == 1 ? EVoxelTaskBlockReason::Dependency : EVoxelTaskBlockReason::None;
				};
				Request.Execute = [&ReleaseWorkers](const TAtomic<bool>& Cancel)
				{
					while (!ReleaseWorkers.Load() && !Cancel.Load()) FPlatformProcess::Sleep(0.001f);
					FVoxelTaskResult Result;
					Result.bCanceled = Cancel.Load();
					Result.bSuccess = !Result.bCanceled;
					return Result;
				};
				TestTrue(TEXT("借位夹具仍通过原 Enqueue"), Scheduler.Enqueue(MoveTemp(Request)));
			}
		}
		const auto Diagnostics = Scheduler.GetDiagnostics();
		TestEqual(TEXT("自动模式只借用空闲类别槽，手动模式保留显式类别上限"),
			Diagnostics.Running, bAutomatic ? 2 : 1);
		TestEqual(TEXT("真实依赖阻塞永远不能因类别借位而开工"), Diagnostics.Pending, bAutomatic ? 1 : 2);
		TestEqual(TEXT("只有真正启动的借位才计数"), Diagnostics.BorrowedKindSlots, bAutomatic ? uint64(1) : uint64(0));
		ReleaseWorkers.Store(true);
		Scheduler.StopAndJoin();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHardwareWorkerLimitTest,
	"WHFramework.Voxel.FineRefactor.Generation.HardwareWorkerLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHardwareWorkerLimitTest::RunTest(const FString& InParameters)
{
	FVoxelTerrainFrameBudgetSettings Settings;
	const int32 LogicalProcessors = FMath::Max(1, FPlatformMisc::NumberOfCoresIncludingHyperthreads());
	Settings.bUseAutomaticWorkerCount = true;
	Settings.ReservedLogicalProcessors = 2;
	Settings.MaximumWorkers = 1;
	const int32 Automatic = FMath::Clamp(LogicalProcessors - 2, 1, FMath::Min(4096, LogicalProcessors));
	TestEqual(TEXT("自动模式读取实际逻辑处理器并扣除系统保留"),
		FVoxelTerrainFrameBudget::ResolveMaximumWorkers(Settings), Automatic);
	Settings.bUseAutomaticWorkerCount = false;
	Settings.MaximumWorkers = 3;
	const int32 Manual = FMath::Min(3, LogicalProcessors);
	TestEqual(TEXT("手动模式保留显式上限"), FVoxelTerrainFrameBudget::ResolveMaximumWorkers(Settings), Manual);
	FVoxelTerrainFrameBudget Budget;
	FString Error;
	if (!TestTrue(TEXT("手动配置有效"), Budget.Configure(Settings, Error)))
	{
		return false;
	}
	FVoxelTerrainFrameObservation Observation;
	Observation.GameThreadMilliseconds = 8.0;
	Observation.TerrainGameThreadMilliseconds = 2.0;
	Observation.RenderThreadMilliseconds = 6.0;
	Observation.GPUMilliseconds = 8.0;
	Observation.bInitialSpawn = true;
	TestEqual(TEXT("出生阶段也不得越过显式上限"), Budget.Observe(Observation).MaximumWorkers, Manual);
	Observation.GPUMilliseconds = 100.0;
	TestEqual(TEXT("单独GPU变慢不会把后台计算强制串行"), Budget.Observe(Observation).MaximumWorkers, Manual);
	return true;
}

#endif
