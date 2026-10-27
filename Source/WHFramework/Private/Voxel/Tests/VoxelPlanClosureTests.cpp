#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationPlanCoordinator.h"
#include "Voxel/Generation/VoxelGenerationPlanningRead.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelLocalPlanClosureTest, "WHFramework.Voxel.FineRefactor.Generation.LocalPlanClosure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelLocalPlanClosureTest::RunTest(const FString& InParameters)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelTaskScheduler Scheduler(true);
	FVoxelGenerationPlanCoordinator Coordinator(Scheduler, Config, Cache, 17);
	FVoxelGenerationPlanDependency Near, Far;
	Near.Kind = Far.Kind = EVoxelGenerationPlanKind::Structure;
	Far.TileKey.Coordinate = FIntVector(400, 0, 0);
	FVoxelGenerationPlanDependencyCapture NearReads, FarReads;
	FString Error;
	TestTrue(TEXT("近处事实具有独立闭包"), Coordinator.GatherPrerequisites(Near, NearReads, Error));
	TestTrue(TEXT("远处事实具有独立闭包"), Coordinator.GatherPrerequisites(Far, FarReads, Error));
	TSet<FVoxelHydrologyRegionKey> ExpectedHydrology;
	for (int32 CellY = 0; CellY < FVoxelGenerationQuery::GenerationPlanTileSide; ++CellY)
	{
		for (int32 CellX = 0; CellX < FVoxelGenerationQuery::GenerationPlanTileSide; ++CellX)
		{
			ExpectedHydrology.Add(FVoxelGenerationQuery::HydrologyKeyForVoxel(CellX, CellY,
				Config->Recipe->Settings, Config->Recipe->AlgorithmVersion));
		}
	}
	TestEqual(TEXT("结构闭包覆盖所属瓦片的全部水文键"), NearReads.Values.Num(), ExpectedHydrology.Num());
	for (const auto& Read : NearReads.Values)
	{
		TestTrue(TEXT("结构闭包不包含瓦片外水文键"), Read.Kind == EVoxelGenerationPlanKind::Hydrology &&
			ExpectedHydrology.Remove(Read.HydrologyKey) == 1);
	}
	TestTrue(TEXT("结构闭包没有遗漏水文键"), ExpectedHydrology.IsEmpty());
	TestFalse(TEXT("远处闭包不别名近处水文"), NearReads.Values.Contains(FarReads.Values[0]));
	for (const auto& Read : NearReads.Values)
	{
		FVoxelHydrologyPlanPtr Plan;
		TestTrue(TEXT("预置近处已完成的事实"), Cache->GetOrBuildHydrology(Read.HydrologyKey,
			[](FVoxelHydrologyPlan&, FString& OutError) { OutError.Reset(); return true; }, Plan, Error));
	}
	const auto Consumer = FVoxelGenerationConsumerKey::Section(17, FGuid::NewGuid(), 3, {}, 1);
	const FVoxelGenerationPlanDependency Facts[] = {Near, Far};
	const auto Status = Coordinator.Ensure(MakeArrayView(Facts), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer);
	TestFalse(TEXT("调用者仍等待完整请求结果"), Status.bReady);
	TestTrue(TEXT("近处计划可在远处水文未完成时排队"), Coordinator.Storage->States.FindChecked(Near).bInFlight);
	TestTrue(TEXT("初次派发即保留原生事实自己的完整已知采样闭包"),
		Coordinator.Storage->States.FindChecked(Near).PrerequisiteRetention.IsValid());
	TestFalse(TEXT("成功派发后不保留未接纳的尝试标记"), Coordinator.Storage->States.FindChecked(Near).bAttemptPendingAdmission);
	TestFalse(TEXT("远处计划等待自己的水文"), Coordinator.Storage->States.FindChecked(Far).bInFlight);
	TestTrue(TEXT("远处水文拥有独立任务"), Coordinator.Storage->States.FindChecked(FarReads.Values[0]).bInFlight);
	const auto* Donated = Coordinator.PriorityGraph.Find(Coordinator.Storage->States.FindChecked(FarReads.Values[0]).PriorityNode);
	TestTrue(TEXT("生产依赖图将完整 Source 优先级送至事实叶子"), Donated && Donated->SourceStableKey == Consumer.OwnerId &&
		Donated->Band == EVoxelTerrainServiceBand::P0);
	const auto PartialKey = FarReads.Values.Last().HydrologyKey;
	Cache->StoreHydrology(PartialKey, MakeShared<FVoxelHydrologyPlan, ESPMode::ThreadSafe>());
	FVoxelGenerationCacheRetention EmptyStreaming;
	EmptyStreaming.Revision = 1;
	Cache->UpdateRetention(EmptyStreaming);
	for (int32 Index = 0; Index < FVoxelGenerationPlanCache::ShardCount * 2; ++Index)
	{
		Cache->TickMaintenance(256);
	}
	{
		FVoxelHydrologyPlanPtr Partial;
		TestTrue(TEXT("等待其他依赖期间，已完成的部分闭包不能被维护回收"), Cache->FindHydrology(PartialKey, Partial));
	}
	Coordinator.ReleaseConsumer(Consumer);
	const double RetirementDeadline = FPlatformTime::Seconds() + 2.;
	do
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {});
		if (Scheduler.ActiveCount()) FPlatformProcess::Sleep(.001f);
	} while (Scheduler.ActiveCount() && FPlatformTime::Seconds() < RetirementDeadline);
	Scheduler.Tick([](FVoxelTaskResult&&) {});
	Scheduler.StopAndJoin();
	for (int32 Index = 0; Index < FVoxelGenerationPlanCache::ShardCount * 2; ++Index)
	{
		Cache->TickMaintenance(256);
	}
	{
		FVoxelHydrologyPlanPtr Partial;
		TestFalse(TEXT("消费者释放且任务退休后，部分闭包恢复正常回收"), Cache->FindHydrology(PartialKey, Partial));
	}
	// 容量置换不是事实撤销；重试必须使用调度器保留年龄的原尝试。
	{
		FVoxelTaskScheduler AdmissionScheduler(true);
		FVoxelTaskBudget Budget;
		Budget.MaxConcurrentTasks = 1;
		Budget.MaxPendingTasks = 1;
		AdmissionScheduler.SetBudget(Budget);
		FVoxelTaskRequest Occupied;
		Occupied.Kind = EVoxelTaskKind::GenerateExactBase;
		Occupied.WorkClass = EVoxelWorkClass::Visible;
		Occupied.Stamp.WorldEpoch = 19;
		Occupied.Stamp.Token = 1;
		Occupied.ReservedBytes = 1024;
		Occupied.Execute = [](const TAtomic<bool>& Cancel)
		{
			while (!Cancel.Load()) FPlatformProcess::Sleep(.001f);
			return FVoxelTaskResult();
		};
		TestTrue(TEXT("占据唯一执行槽"), AdmissionScheduler.Enqueue(MoveTemp(Occupied)));
		const auto AdmissionCache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
		FVoxelGenerationPlanCoordinator AdmissionCoordinator(AdmissionScheduler, Config, AdmissionCache, 19);
		FVoxelGenerationPlanDependency Hydro;
		Hydro.Kind = EVoxelGenerationPlanKind::Hydrology;
		const auto AdmissionConsumer = FVoxelGenerationConsumerKey::Section(19, FGuid::NewGuid(), 1, {}, 1);
		const FVoxelGenerationPlanDependency Reads[] = {Hydro};
		AdmissionCoordinator.Ensure(MakeArrayView(Reads), EVoxelWorkClass::Critical, 3, 1000., 0., AdmissionConsumer);
		const uint64 OriginalAttempt = AdmissionCoordinator.Storage->States.FindChecked(Hydro).AttemptId;
		FVoxelTaskRequest Urgent;
		Urgent.Kind = EVoxelTaskKind::GenerateExactBase;
		Urgent.WorkClass = EVoxelWorkClass::Critical;
		Urgent.Stamp.WorldEpoch = 19;
		Urgent.Stamp.Token = 2;
		Urgent.ReservedBytes = 1024;
		Urgent.Execute = [](const TAtomic<bool>&) { return FVoxelTaskResult(); };
		const FVoxelTaskStamp UrgentStamp = Urgent.Stamp;
		TestTrue(TEXT("紧急任务置换等待中的原生事实"), AdmissionScheduler.Enqueue(MoveTemp(Urgent)));
		AdmissionScheduler.Tick([](FVoxelTaskResult&&) {});
		TestTrue(TEXT("置换保留事实的原尝试身份"), AdmissionCoordinator.Storage->States.FindChecked(Hydro).bAttemptPendingAdmission);
		AdmissionCoordinator.Ensure(MakeArrayView(Reads), EVoxelWorkClass::Critical, 3, 1000., 0., AdmissionConsumer);
		{
			auto& Waiting = AdmissionCoordinator.Storage->States.FindChecked(Hydro);
			if (!TestTrue(TEXT("满队列拒绝后保留原请求及独立队列等待"), Waiting.PendingRequest.IsSet() &&
				Waiting.bWaitingTaskCapacity && Waiting.bWaitingTaskQueueCapacity)) return false;
			const uint64 InitialDeferrals = AdmissionScheduler.GetDiagnostics().CapacityDeferrals;
			const uint64 QueueVersion = AdmissionScheduler.GetQueueAdmissionVersion();
			const auto& Pending = Waiting.PendingRequest.GetValue();
			TestTrue(TEXT("Ensure 的后续优先级规范化使原排队快照失效"), QueueVersion > Waiting.TaskQueueVersion);
			TestEqual(TEXT("首次检查前账本版本与拒收快照一致"), AdmissionScheduler.GetTerrainResources()->Snapshot().Version,
				Waiting.TaskQueueResourceVersion);
			TestTrue(TEXT("首次检查前请求自身优先级与拒收快照一致"), Pending.WorkClass == Waiting.TaskQueueWorkClass &&
				Pending.InputBytes == Waiting.TaskQueueInputBytes &&
				FVoxelTaskScheduler::ResolveServicePriority(Pending) == Waiting.TaskQueuePriority);
			TestTrue(TEXT("首次检查前聚合优先级与拒收快照一致"), Waiting.WorkClass == Waiting.TaskQueueWorkClass &&
				Waiting.ServicePriority == Waiting.TaskQueuePriority && Waiting.SourcePriority == Waiting.TaskQueuePriority.SourcePriority &&
				Waiting.DistanceScore == Waiting.TaskQueuePriority.DistanceCells && Waiting.ForwardScore == Waiting.TaskQueuePriority.ForwardScore);
			TestTrue(TEXT("首次检查前年龄截止尚未到期"), FPlatformTime::Seconds() < Waiting.TaskQueueRetryAt);
			TestFalse(TEXT("排队版本变化后执行一次实际准入检查"), AdmissionCoordinator.Queue(Hydro, Waiting));
			TestEqual(TEXT("版本变化触发一次真实重试"), AdmissionScheduler.GetDiagnostics().CapacityDeferrals, InitialDeferrals + 1);
			TestEqual(TEXT("重试后的排队快照已更新"), Waiting.TaskQueueVersion, AdmissionScheduler.GetQueueAdmissionVersion());
			const uint64 Deferrals = AdmissionScheduler.GetDiagnostics().CapacityDeferrals;
			for (int32 Retry = 0; Retry < 4; ++Retry)
			{
				TestFalse(TEXT("不变的队列与授额不重复接纳"), AdmissionCoordinator.Queue(Hydro, Waiting));
			}
			TestEqual(TEXT("不变快照不重新扫描满队列"), AdmissionScheduler.GetDiagnostics().CapacityDeferrals, Deferrals);
			TestEqual(TEXT("只读重试不改变队列版本"), AdmissionScheduler.GetQueueAdmissionVersion(), QueueVersion);
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Pressure;
			const auto PressureConsumer = FVoxelGenerationConsumerKey::Section(19, FGuid::NewGuid(), 1, {}, 99);
			if (!TestTrue(TEXT("原账本资源变化可被观测"), AdmissionScheduler.GetTerrainResources()->Acquire(
				PressureConsumer, EVoxelTerrainResourcePool::Input, 1, Pressure))) return false;
			TestFalse(TEXT("授额变化后重新检查真实准入"), AdmissionCoordinator.Queue(Hydro, Waiting));
			TestEqual(TEXT("授额变化不能隐藏排队检查"), AdmissionScheduler.GetDiagnostics().CapacityDeferrals, Deferrals + 1);
			Pressure.Reset();
			TestFalse(TEXT("授额释放后仍按原条件检查满队列"), AdmissionCoordinator.Queue(Hydro, Waiting));
			TestEqual(TEXT("授额释放不被旧快照屏蔽"), AdmissionScheduler.GetDiagnostics().CapacityDeferrals, Deferrals + 2);
			AdmissionScheduler.UpdatePriorities([UrgentStamp](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp,
				EVoxelWorkClass&, int32&, double& Distance, double&)
			{
				if (Kind == EVoxelTaskKind::GenerateExactBase && Stamp == UrgentStamp) Distance += 1.0;
			});
			TestFalse(TEXT("已排队对手优先级变化后重新比较"), AdmissionCoordinator.Queue(Hydro, Waiting));
			TestEqual(TEXT("队列优先级变化不被旧快照屏蔽"), AdmissionScheduler.GetDiagnostics().CapacityDeferrals, Deferrals + 3);
			TestTrue(TEXT("年龄截止由原首次请求时间计算"), Waiting.TaskQueueRetryAt == MAX_dbl ||
				Waiting.TaskQueueRetryAt == Waiting.PendingRequest.GetValue().QueuedAt + 0.5);
			Waiting.TaskQueueRetryAt = FPlatformTime::Seconds() - 1.0;
			TestFalse(TEXT("年龄截止到期会重试而不是永久阻塞"), AdmissionCoordinator.Queue(Hydro, Waiting));
			TestEqual(TEXT("到期后实际重新执行原准入"), AdmissionScheduler.GetDiagnostics().CapacityDeferrals, Deferrals + 4);
			Waiting.WorkClass = EVoxelWorkClass::Critical;
			Waiting.SourcePriority = -1;
			Waiting.DistanceScore = 0.0;
			Waiting.ServicePriority.Band = EVoxelTerrainServiceBand::P0;
			Waiting.ServicePriority.SourcePriority = -1;
			Waiting.ServicePriority.DistanceCells = 0.0;
			TestTrue(TEXT("优先级提升立即重试并可置换较低优先级"), AdmissionCoordinator.Queue(Hydro, Waiting));
		}
		TestFalse(TEXT("高优先级事实接纳后原满队列项被置换"), AdmissionScheduler.Has(UrgentStamp, EVoxelTaskKind::GenerateExactBase));
		const uint64 BeforePriority = AdmissionScheduler.GetQueueAdmissionVersion();
		AdmissionScheduler.UpdatePriorities([](EVoxelTaskKind Kind, const FVoxelTaskStamp&, EVoxelWorkClass&, int32&, double& Distance, double&)
		{
			if (Kind == EVoxelTaskKind::BuildGenerationPlan) Distance += 1.0;
		});
		TestTrue(TEXT("排队任务优先级实际变化使快照失效"), AdmissionScheduler.GetQueueAdmissionVersion() > BeforePriority);
		const uint64 BeforeSameBudget = AdmissionScheduler.GetQueueAdmissionVersion();
		AdmissionScheduler.SetBudget(Budget);
		TestEqual(TEXT("重复相同预算不使快照失效"), AdmissionScheduler.GetQueueAdmissionVersion(), BeforeSameBudget);
		Budget.MaxPendingTasks = 2;
		AdmissionScheduler.SetBudget(Budget);
		TestTrue(TEXT("排队预算实际变化使快照失效"), AdmissionScheduler.GetQueueAdmissionVersion() > BeforeSameBudget);
		AdmissionScheduler.CancelTask(EVoxelTaskKind::GenerateExactBase, UrgentStamp);
		AdmissionScheduler.Tick([](FVoxelTaskResult&&) {});
		AdmissionCoordinator.Ensure(MakeArrayView(Reads), EVoxelWorkClass::Critical, 3, 1000., 0., AdmissionConsumer);
		TestEqual(TEXT("置换重试不产生新token或遗留旧准入记录"), AdmissionCoordinator.Storage->States.FindChecked(Hydro).AttemptId, OriginalAttempt);
		TestTrue(TEXT("原事实重新进入有界队列"), AdmissionCoordinator.Storage->States.FindChecked(Hydro).bInFlight);
		const uint64 BeforeRelease = AdmissionScheduler.GetQueueAdmissionVersion();
		AdmissionCoordinator.ReleaseConsumer(AdmissionConsumer);
		TestTrue(TEXT("消费者释放取消排队事实并更新队列版本"), AdmissionScheduler.GetQueueAdmissionVersion() > BeforeRelease);
		AdmissionScheduler.StopAndJoin();
	}
	{
		FVoxelTaskScheduler QueueScheduler(true);
		FVoxelTaskBudget Budget;
		Budget.MaxConcurrentTasks = 1;
		Budget.MaxPendingTasks = 1;
		QueueScheduler.SetBudget(Budget);
		auto MakeRequest = [](const uint64 Token, const EVoxelWorkClass WorkClass)
		{
			FVoxelTaskRequest Request;
			Request.Kind = EVoxelTaskKind::GenerateExactBase;
			Request.WorkClass = WorkClass;
			Request.Stamp.WorldEpoch = 20;
			Request.Stamp.Token = Token;
			Request.ReservedBytes = 1024;
			Request.Execute = [](const TAtomic<bool>& Cancel)
			{
				while (!Cancel.Load()) FPlatformProcess::Sleep(.001f);
				return FVoxelTaskResult();
			};
			return Request;
		};
		TestTrue(TEXT("队列唤醒夹具占据执行槽"), QueueScheduler.Enqueue(MakeRequest(10, EVoxelWorkClass::Visible)));
		auto Blocker = MakeRequest(11, EVoxelWorkClass::Critical);
		const FVoxelTaskStamp BlockerStamp = Blocker.Stamp;
		TestTrue(TEXT("队列唤醒夹具占满排队槽"), QueueScheduler.Enqueue(MoveTemp(Blocker)));
		const auto QueueCache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
		FVoxelGenerationPlanCoordinator QueueCoordinator(QueueScheduler, Config, QueueCache, 20);
		FVoxelGenerationPlanDependency Hydro;
		Hydro.Kind = EVoxelGenerationPlanKind::Hydrology;
		const auto QueueConsumer = FVoxelGenerationConsumerKey::Section(20, FGuid::NewGuid(), 1, {}, 1);
		QueueCoordinator.Ensure(MakeArrayView(&Hydro, 1), EVoxelWorkClass::Critical, 3, 1000., 0., QueueConsumer);
		auto& Waiting = QueueCoordinator.Storage->States.FindChecked(Hydro);
		if (!TestTrue(TEXT("事实确实因满队列等待"), Waiting.PendingRequest.IsSet() && Waiting.bWaitingTaskQueueCapacity)) return false;
		const uint64 PreviousVersion = QueueScheduler.GetQueueAdmissionVersion();
		const uint64 PreviousResources = QueueScheduler.GetTerrainResources()->Snapshot().Version;
		QueueScheduler.CancelTask(EVoxelTaskKind::GenerateExactBase, BlockerStamp);
		TestTrue(TEXT("取消排队任务改变准入版本"), QueueScheduler.GetQueueAdmissionVersion() > PreviousVersion);
		TestEqual(TEXT("取消回执未处理时不依赖原账本版本变化"), QueueScheduler.GetTerrainResources()->Snapshot().Version, PreviousResources);
		TestTrue(TEXT("队列槽释放后原事实立即重新接纳"), QueueCoordinator.Queue(Hydro, Waiting));
		QueueCoordinator.ReleaseConsumer(QueueConsumer);
		QueueScheduler.StopAndJoin();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelEnvironmentAttemptIsolationTest, "WHFramework.Voxel.FineRefactor.Generation.EnvironmentAttemptIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEnvironmentAttemptIsolationTest::RunTest(const FString& InParameters)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	FVoxelTaskScheduler Scheduler(true);
	FVoxelGenerationPlanCoordinator Coordinator(Scheduler, Config, MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>(), 19);
	const FVoxelGenerationBounds Column{FIntVector(-1, 8, 0), FIntVector(-1, 8, 0)};
	const auto First = Coordinator.EnvironmentConsumer(Column, true);
	TestTrue(TEXT("环境重复请求保持同一在途身份"), First == Coordinator.EnvironmentConsumer(Column, true));
	FVoxelGenerationPlanDependency Fact;
	Coordinator.Ensure(MakeArrayView(&Fact, 1), EVoxelWorkClass::Interactive, 0, 0.0, 0.0, First);
	Coordinator.ReleaseEnvironment(FIntPoint(-1, 8));
	const auto Second = Coordinator.EnvironmentConsumer(Column, true);
	TestTrue(TEXT("释放后新请求取得严格递增的尝试代"), Second.RequestSerial > First.RequestSerial);
	TestFalse(TEXT("旧代已从完整订阅表移除"), Coordinator.Storage->Consumers.Contains(First));
	TestFalse(TEXT("新代不能被旧代图节点复用"), Coordinator.Storage->Consumers.Contains(First));
	Coordinator.ReleaseEnvironment(FIntPoint(-1, 8));
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPlanningReadScopeTest, "WHFramework.Voxel.FineRefactor.Generation.PlanningReadScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPlanningReadScopeTest::RunTest(const FString& InParameters)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	int32 OuterReads = 0;
	int32 InnerReads = 0;
	FVoxelGenerationPlanDependency Actual;
	FString Error;
	{
		const auto ObserveOuter = [&](EVoxelGenerationPlanKind Kind, const FIntVector& Coordinate)
		{
			++OuterReads;
			Actual.Kind = Kind;
			Actual.TileKey.Coordinate = Coordinate;
		};
		const FVoxelGenerationPlanningReadScope::FObserver OuterObserver(ObserveOuter);
		FVoxelGenerationPlanningReadScope Scope(&OuterObserver);
		FVoxelGenerationQuery Query;
		TestTrue(TEXT("普通 Query 构造受 Worker 观察范围约束"), FVoxelGenerationQuery::Create(Config, Cache, Query, Error));
		FVoxelCavePlanPtr Cave;
		TestFalse(TEXT("缺失洞穴不能在项目 Worker 内同步构建"), Query.EnsureCavePlan({FIntVector(7, -3, 2)}, Cave, Error));
		TestTrue(TEXT("返回可等待的实际依赖键"), Error.StartsWith(TEXT("DependencyNotReady")) &&
			Actual.Kind == EVoxelGenerationPlanKind::Cave && Actual.Coordinate() == FIntVector(7, -3, 2));
		TestFalse(TEXT("Cache 中没有隐式生成的洞穴事实"), Cache->FindCave({FIntVector(7, -3, 2)}, Cave));
		{
			const auto ObserveInner = [&](EVoxelGenerationPlanKind, const FIntVector&) { ++InnerReads; };
			const FVoxelGenerationPlanningReadScope::FObserver InnerObserver(ObserveInner);
			FVoxelGenerationPlanningReadScope Nested(&InnerObserver);
			FVoxelHydrologyPlanPtr Hydro;
			TestFalse(TEXT("嵌套观察仍禁止同步水文构建"), Query.EnsureHydrologyPlan({FIntPoint(9, 10)}, Hydro, Error));
		}
		Query.EnsureCavePlan({FIntVector(7, -3, 2)}, Cave, Error);
	}
	TestEqual(TEXT("嵌套采样通知其直接观察者"), InnerReads, 1);
	TestEqual(TEXT("调用者保留完整嵌套实际读集并恢复观察者"), OuterReads, 3);
	TestFalse(TEXT("Worker 返回后不污染线程后续自然生成任务"), FVoxelGenerationPlanningReadScope::IsActive());
	FVoxelTaskStamp Stamp;
	Stamp.OwnerId = FGuid::NewGuid();
	TAtomic<bool> Cancel(false);
	{
		FVoxelGenerationPlanningReadScope Outer(nullptr, &Stamp, &Cancel);
		{
			FVoxelGenerationPlanningReadScope Inner(nullptr);
			TestTrue(TEXT("无新Owner的嵌套采样继承完整任务Stamp"), FVoxelGenerationPlanningReadScope::GetTaskStamp() == &Stamp);
			Cancel.Store(true);
			TestFalse(TEXT("嵌套范围不能忽略父任务撤销后发布缓存"), FVoxelGenerationPlanningReadScope::CanPublish());
			TestEqual(TEXT("父任务撤销返回Canceled"), FVoxelGenerationPlanningReadScope::PublishBlockError(), FString(TEXT("Canceled")));
			Cancel.Store(false);
			FVoxelGenerationPlanningReadScope::Missing(EVoxelGenerationPlanKind::Cave, FIntVector(9, 2, -1));
		}
		TestFalse(TEXT("子采样缺失阻止外层任务发布不完整缓存"), FVoxelGenerationPlanningReadScope::CanPublish());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelDependencyPriorityCapacityTest,
	"WHFramework.Voxel.FineRefactor.Generation.DependencyPriorityCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelDependencyPriorityCapacityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(59, FGuid::NewGuid(), 1, {}, 1);
	FVoxelTerrainDependencyPriority Graph(Resources, Consumer);
	FVoxelTerrainServicePriority Critical;
	Critical.Band = EVoxelTerrainServiceBand::P0;
	Critical.DistanceCells = 1;
	Critical.ForwardScore = 1;
	Critical.SourcePriority = 0;
	Critical.SourceStableKey = Consumer.OwnerId;
	FVoxelTerrainServicePriority Background = Critical;
	Background.Band = EVoxelTerrainServiceBand::P6;
	Background.DistanceCells = 1000;
	const FGuid Root = FGuid::NewGuid();
	const FGuid Left = FGuid::NewGuid();
	const FGuid Right = FGuid::NewGuid();
	const FGuid Leaf = FGuid::NewGuid();
	FString Error;
	if (!Graph.Register(Root, Critical, Error) || !Graph.Register(Left, Background, Error) ||
		!Graph.Register(Right, Background, Error) || !Graph.Register(Leaf, Background, Error)) return false;
	const TSet<FGuid> LeftEdge{Left};
	const TSet<FGuid> RightEdge{Right};
	const TSet<FGuid> LeafEdge{Leaf};
	if (!Graph.SetDependencies(Root, LeftEdge, Error) || !Graph.SetDependencies(Left, LeafEdge, Error)) return false;
	Graph.Advance(128, [](const FGuid&, const FVoxelTerrainServicePriority&) {});
	TestEqual(TEXT("Production graph propagates a transitive donation"), uint8(Graph.Find(Leaf)->Band), uint8(EVoxelTerrainServiceBand::P0));
	const uint64 OriginalLease = Graph.Storage->Nodes.FindChecked(Root).DependenciesLease->Serial();
	TestFalse(TEXT("Production cycle search rejects leaf to root"), Graph.SetDependencies(Leaf, TSet<FGuid>{Root}, Error));
	TestTrue(TEXT("Cycle rejection preserves accepted topology"), Graph.Storage->Nodes.FindChecked(Leaf).Dependencies.IsEmpty());
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WorkingBlocker;
	const auto BeforeWorking = Resources->Snapshot();
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
		BeforeWorking.Limits.WorkingBytes - BeforeWorking.Used[uint8(EVoxelTerrainResourcePool::Working)], WorkingBlocker)) return false;
	TestFalse(TEXT("Ungrantable reverse-edge plan suspends replacement"), Graph.SetDependencies(Root, RightEdge, Error));
	TestTrue(TEXT("Failure is capacity waiting"), Error.StartsWith(TEXT("CapacityBlocked")));
	TestTrue(TEXT("Capacity denial preserves original dependency"), Graph.Storage->Nodes.FindChecked(Root).Dependencies.Contains(Left));
	TestEqual(TEXT("Capacity denial preserves original dependency lease"), Graph.Storage->Nodes.FindChecked(Root).DependenciesLease->Serial(), OriginalLease);
	TestEqual(TEXT("Capacity denial preserves current effective priority"), uint8(Graph.Find(Leaf)->Band), uint8(EVoxelTerrainServiceBand::P0));
	WorkingBlocker.Reset();
	if (!Graph.SetDependencies(Root, RightEdge, Error)) return false;
	Graph.Advance(128, [](const FGuid&, const FVoxelTerrainServicePriority&) {});
	TestEqual(TEXT("Accepted replacement promotes new dependency"), uint8(Graph.Find(Right)->Band), uint8(EVoxelTerrainServiceBand::P0));
	TestEqual(TEXT("Accepted replacement retracts old transitive donation"), uint8(Graph.Find(Leaf)->Band), uint8(EVoxelTerrainServiceBand::P6));
	const uint64 ChargedInput = Resources->Snapshot().Used[uint8(EVoxelTerrainResourcePool::Input)];
	const uint64 Backing = VoxelGenerationAllocation::AllocatedBytes(Graph.Storage->Nodes);
	TestTrue(TEXT("Persistent node backing is charged"), Backing <= Graph.Storage->NodesLease->Bytes() && Backing < ChargedInput);
	for (int32 Index = 0; Index < 1024; ++Index)
	{
		const FGuid Temporary = FGuid::NewGuid();
		if (!Graph.Register(Temporary, Background, Error)) return false;
		Graph.Remove(Temporary);
	}
	TestFalse(TEXT("Remove unlinks queued nodes instead of retaining historical work"), Graph.IsPending());
	const int32 OriginalNodes = Graph.Storage->Nodes.Num();
	Graph.SetPriority(Left, Critical);
	Graph.Advance(128, [&](const FGuid& Key, const FVoxelTerrainServicePriority&)
	{
		if (Key != Left) return;
		Graph.Remove(Left);
		for (int32 Index = 0; Index < 32; ++Index)
		{
			FString RegisterError;
			TestTrue(TEXT("Reentrant callback may grow node backing"), Graph.Register(FGuid::NewGuid(), Background, RegisterError));
		}
	});
	TestTrue(TEXT("Reentrant growth consumed no dangling node or dependency references"), Graph.Storage->Nodes.Num() >= OriginalNodes + 31);
	while (Graph.Storage->Nodes.Num() < Graph.Storage->Capacity)
	{
		if (!Graph.Register(FGuid::NewGuid(), Background, Error)) return false;
	}
	const FGuid DeniedNode = FGuid::NewGuid();
	const int32 BeforeCount = Graph.Storage->Nodes.Num();
	const uint64 BeforeNodeLease = Graph.Storage->NodesLease->Serial();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputBlocker;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, 1, InputBlocker)) return false;
	const auto BeforeInput = Resources->Snapshot();
	if (!InputBlocker->Resize(1 + BeforeInput.Limits.InputBytes - BeforeInput.Used[uint8(EVoxelTerrainResourcePool::Input)])) return false;
	TestFalse(TEXT("Node growth requires old and new backing grants"), Graph.Register(DeniedNode, Background, Error));
	TestTrue(TEXT("Denied node is absent"), Graph.Find(DeniedNode) == nullptr);
	TestEqual(TEXT("Denied node growth retains current map"), Graph.Storage->Nodes.Num(), BeforeCount);
	TestEqual(TEXT("Denied node growth retains current lease"), Graph.Storage->NodesLease->Serial(), BeforeNodeLease);
	InputBlocker.Reset();
	if (!Graph.Register(DeniedNode, Background, Error)) return false;
	auto OldStorage = Graph.Storage;
	Graph.Reset();
	TestTrue(TEXT("Reset detaches current graph immediately"), Graph.Find(Root) == nullptr && !Graph.IsPending());
	TestTrue(TEXT("Old storage retains backing and node leases until actual last reader release"), OldStorage->NodesLease && OldStorage->Nodes.Num() > 0);
	if (!Graph.Register(FGuid::NewGuid(), Background, Error)) return false;
	const uint64 BeforeRetirement = Resources->Snapshot().Used[uint8(EVoxelTerrainResourcePool::Input)];
	OldStorage.Reset();
	const double Deadline = FPlatformTime::Seconds() + 2.0;
	while (Resources->Snapshot().Used[uint8(EVoxelTerrainResourcePool::Input)] >= BeforeRetirement && FPlatformTime::Seconds() < Deadline)
		FPlatformProcess::SleepNoStats(0.001f);
	TestTrue(TEXT("Actual worker retirement returns old graph bytes"), Resources->Snapshot().Used[uint8(EVoxelTerrainResourcePool::Input)] < BeforeRetirement);
	Graph.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelGenerationGraphAdmissionTest,
	"WHFramework.Voxel.FineRefactor.Generation.GraphAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelGenerationGraphAdmissionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelTaskScheduler Scheduler(true);
	FVoxelGenerationPlanCoordinator Coordinator(Scheduler, Config, Cache, 61);
	FVoxelGenerationPlanDependency First;
	FVoxelGenerationPlanDependency Second;
	Second.HydrologyKey.Coordinate = FIntPoint(7, -3);
	FString Error;
	for (const auto& Fact : {First, Second})
	{
		FVoxelHydrologyPlanPtr Plan;
		if (!Cache->GetOrBuildHydrology(Fact.HydrologyKey,
			[](FVoxelHydrologyPlan&, FString& OutError) { OutError.Reset(); return true; }, Plan, Error)) return false;
	}
	const auto Consumer = FVoxelGenerationConsumerKey::Section(61, FGuid::NewGuid(), 1, {}, 1);
	if (!TestTrue(TEXT("Original cached fact is admitted through production Ensure"), Coordinator.Ensure(
		MakeArrayView(&First, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer).bReady)) return false;
	const uint64 Subscription = Coordinator.Storage->Consumers.FindChecked(Consumer).Dependencies.FindChecked(First).Id;
	const FGuid ConsumerNode = Coordinator.Storage->Consumers.FindChecked(Consumer).PriorityNode;
	const auto Resources = Scheduler.GetRuntimeResources();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WorkingBlocker;
	const auto BeforeWorking = Resources->Snapshot();
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
		BeforeWorking.Limits.WorkingBytes - BeforeWorking.Used[uint8(EVoxelTerrainResourcePool::Working)], WorkingBlocker)) return false;
	FVoxelGenerationPlanDependency Cold;
	Cold.Kind = EVoxelGenerationPlanKind::Structure;
	Cold.TileKey.Coordinate = FIntVector(400, -30, 0);
	const int32 BeforeColdStates = Coordinator.Storage->States.Num();
	const int32 BeforeColdFacts = Coordinator.Storage->PriorityFacts.Num();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const auto ColdStatus = Coordinator.Ensure(MakeArrayView(&Cold, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer);
		TestTrue(TEXT("Cold closure is suspended by actual Working lease pressure"), !ColdStatus.bReady && !ColdStatus.bFailed && ColdStatus.Error.StartsWith(TEXT("CapacityBlocked")));
		TestEqual(TEXT("Failed closure rolls back unowned fact states"), Coordinator.Storage->States.Num(), BeforeColdStates);
		TestEqual(TEXT("Repeated rejection leaves no orphan priority facts"), Coordinator.Storage->PriorityFacts.Num(), BeforeColdFacts);
	}
	const auto Blocked = Coordinator.Ensure(MakeArrayView(&Second, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer);
	TestFalse(TEXT("Graph grant failure is not Ready"), Blocked.bReady);
	TestFalse(TEXT("Graph grant failure is not terminal fact failure"), Blocked.bFailed);
	TestTrue(TEXT("Graph grant failure exposes capacity reason"), Blocked.Error.StartsWith(TEXT("CapacityBlocked")));
	TestEqual(TEXT("Failed topology retains old subscription"), Coordinator.Storage->Consumers.FindChecked(Consumer).Dependencies.FindChecked(First).Id, Subscription);
	TestEqual(TEXT("Failed topology retains original consumer node"), Coordinator.Storage->Consumers.FindChecked(Consumer).PriorityNode, ConsumerNode);
	TestFalse(TEXT("Rejected first subscription removes its newly registered fact"), Coordinator.Storage->States.Contains(Second));
	WorkingBlocker.Reset();
	if (!TestTrue(TEXT("Original consumer resumes new requested topology"), Coordinator.Ensure(
		MakeArrayView(&Second, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer).bReady)) return false;
	TestFalse(TEXT("Accepted topology withdraws old subscription"), Coordinator.Storage->Consumers.FindChecked(Consumer).Dependencies.Contains(First));
	TestTrue(TEXT("Accepted topology records new subscription"), Coordinator.Storage->Consumers.FindChecked(Consumer).Dependencies.Contains(Second));
	TArray<FVoxelGenerationConsumerKey> OtherConsumers;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const auto Other = FVoxelGenerationConsumerKey::Section(61, FGuid::NewGuid(), 1, {}, 1);
		OtherConsumers.Add(Other);
		if (!Coordinator.Ensure(MakeArrayView(&Second, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Other).bReady) return false;
	}
	const auto BeforeSecondWorking = Resources->Snapshot();
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
		BeforeSecondWorking.Limits.WorkingBytes - BeforeSecondWorking.Used[uint8(EVoxelTerrainResourcePool::Working)], WorkingBlocker)) return false;
	const auto Denied = FVoxelGenerationConsumerKey::Section(61, FGuid::NewGuid(), 1, {}, 1);
	const auto DeniedStatus = Coordinator.Ensure(MakeArrayView(&Second, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Denied);
	TestTrue(TEXT("New consumer reaches reverse-edge capacity wait"), !DeniedStatus.bReady && !DeniedStatus.bFailed && DeniedStatus.Error.StartsWith(TEXT("CapacityBlocked")));
	TestTrue(TEXT("Waiting consumer has only its original control node"), Coordinator.Storage->Consumers.Contains(Denied));
	TestFalse(TEXT("Denied consumer has no accepted subscription"), !Coordinator.Storage->Consumers.FindChecked(Denied).Dependencies.IsEmpty());
	Coordinator.ReleaseConsumer(Denied);
	TestFalse(TEXT("Cancellation removes control node even before first subscription"), Coordinator.Storage->Consumers.Contains(Denied));
	WorkingBlocker.Reset();
	if (!Coordinator.PreparePrerequisites(Coordinator.Storage->States.FindChecked(Second), 1, Error)) return false;
	Coordinator.Storage->States.FindChecked(Second).Prerequisites.Add(Second);
	const auto CycleStatus = Coordinator.Ensure(MakeArrayView(&Second, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer);
	TestTrue(TEXT("Stored read cycle is rejected before recursive re-registration"), CycleStatus.bFailed && CycleStatus.Error.Contains(TEXT("循环")));
	TestFalse(TEXT("Rejected cycle clears its registration marker"), Coordinator.Storage->States.FindChecked(Second).bRegistering);
	TestTrue(TEXT("Rejected read cycle retains accepted subscription"), Coordinator.Storage->Consumers.FindChecked(Consumer).Dependencies.Contains(Second));
	Coordinator.Storage->States.FindChecked(Second).Prerequisites.Reset();
	FVoxelGenerationPlanDependency Middle;
	Middle.Kind = EVoxelGenerationPlanKind::Structure;
	FVoxelGenerationPlanDependency Outer;
	Outer.Kind = EVoxelGenerationPlanKind::Feature;
	FVoxelStructurePlanPtr Structure;
	FVoxelFeaturePlanPtr Feature;
	if (!Cache->GetOrBuildStructure(Middle.TileKey,
		[](FVoxelStructurePlan&, FString& OutError) { OutError.Reset(); return true; }, Structure, Error) ||
		!Cache->GetOrBuildFeature(Outer.TileKey,
			[](FVoxelFeaturePlan&, FString& OutError) { OutError.Reset(); return true; }, Feature, Error)) return false;
	const auto MiddleConsumer = FVoxelGenerationConsumerKey::Section(61, FGuid::NewGuid(), 1, {}, 1);
	const auto OuterConsumer = FVoxelGenerationConsumerKey::Section(61, FGuid::NewGuid(), 1, {}, 1);
	if (!Coordinator.Ensure(MakeArrayView(&Middle, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, MiddleConsumer).bReady ||
		!Coordinator.Ensure(MakeArrayView(&Outer, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, OuterConsumer).bReady) return false;
	const uint64 LeafSubscription = Coordinator.Storage->Consumers.FindChecked(Consumer).Dependencies.FindChecked(Second).Id;
	if (!Coordinator.PreparePrerequisites(Coordinator.Storage->States.FindChecked(Middle),
		Coordinator.Storage->States.FindChecked(Middle).Prerequisites.Num() + 1, Error) ||
		!Coordinator.PreparePrerequisites(Coordinator.Storage->States.FindChecked(Outer),
			Coordinator.Storage->States.FindChecked(Outer).Prerequisites.Num() + 1, Error)) return false;
	Coordinator.Storage->States.FindChecked(Middle).Prerequisites.Add(Second);
	Coordinator.Storage->States.FindChecked(Outer).Prerequisites.Add(Middle);
	if (!Coordinator.Ensure(MakeArrayView(&Outer, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer).bReady) return false;
	const auto& Accepted = Coordinator.Storage->Consumers.FindChecked(Consumer).Dependencies;
	TestTrue(TEXT("Consumer subscribes the dynamically extended middle fact"), Accepted.Contains(Middle));
	TestEqual(TEXT("Transitive replacement retains original leaf subscription identity"), Accepted.FindChecked(Second).Id, LeafSubscription);
	for (const auto& Prerequisite : Coordinator.Storage->States.FindChecked(Middle).Prerequisites)
		TestTrue(TEXT("Consumer retains every transitive leaf subscription"), Accepted.Contains(Prerequisite));
	Coordinator.ReleaseConsumer(Consumer);
	Coordinator.ReleaseConsumer(MiddleConsumer);
	Coordinator.ReleaseConsumer(OuterConsumer);
	for (const auto& Other : OtherConsumers) Coordinator.ReleaseConsumer(Other);
	Coordinator.Reset();
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCoordinatorStorageCapacityTest,
	"WHFramework.Voxel.FineRefactor.Generation.CoordinatorStorageCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCoordinatorStorageCapacityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelTaskScheduler Scheduler(true);
	FVoxelGenerationPlanCoordinator Coordinator(Scheduler, Config, Cache, 67);
	const auto Resources = Scheduler.GetRuntimeResources();
	FVoxelGenerationPlanDependency First;
	FVoxelGenerationPlanDependency Second;
	Second.HydrologyKey.Coordinate = FIntPoint(7, -3);
	FString Error;
	for (const auto& Fact : {First, Second})
	{
		FVoxelHydrologyPlanPtr Plan;
		if (!Cache->GetOrBuildHydrology(Fact.HydrologyKey,
			[](FVoxelHydrologyPlan&, FString& OutError) { OutError.Reset(); return true; }, Plan, Error)) return false;
	}
	const auto Consumer = FVoxelGenerationConsumerKey::Section(67, FGuid::NewGuid(), 1, {}, 1);
	const auto Other = FVoxelGenerationConsumerKey::Section(67, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	const auto Saturate = [&]()
	{
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, 1, Blocker)) return false;
		const auto Snapshot = Resources->Snapshot();
		return Blocker->Resize(1 + Snapshot.Limits.InputBytes - Snapshot.Used[uint8(EVoxelTerrainResourcePool::Input)]);
	};
	if (!Saturate()) return false;
	const auto NoStorage = Coordinator.Ensure(MakeArrayView(&First, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer);
	TestTrue(TEXT("Storage admission is a capacity wait"), !NoStorage.bReady && !NoStorage.bFailed && NoStorage.Error.StartsWith(TEXT("CapacityBlocked")));
	TestTrue(TEXT("Rejected storage creates no uncharged owner"), Coordinator.Storage == nullptr);
	const auto NoEnvironment = Coordinator.EnsureEnvironment(FIntPoint::ZeroValue);
	TestTrue(TEXT("Environment identity allocation denial remains retryable"), !NoEnvironment.bReady && !NoEnvironment.bFailed);
	Blocker.Reset();
	if (!Coordinator.Ensure(MakeArrayView(&First, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer).bReady ||
		!Coordinator.Ensure(MakeArrayView(&Second, 1), EVoxelWorkClass::Visible, 1, 32.0, 0.0, Other).bReady) return false;
	const auto& Body = *Coordinator.Storage;
	TestTrue(TEXT("States backing is granted"), VoxelGenerationAllocation::AllocatedBytes(Body.States) <= Body.StatesLease->Bytes());
	TestTrue(TEXT("Consumers backing is granted"), VoxelGenerationAllocation::AllocatedBytes(Body.Consumers) <= Body.ConsumersLease->Bytes());
	TestTrue(TEXT("Fact identity backing is granted"), VoxelGenerationAllocation::AllocatedBytes(Body.PriorityFacts) <= Body.FactsLease->Bytes());
	const auto& Original = Body.Consumers.FindChecked(Consumer);
	const uint64 Id = Original.Dependencies.FindChecked(First).Id;
	const uint64 EdgeLease = Original.DependenciesLease->Serial();
	const uint64 DonationLease = Body.States.FindChecked(First).DonationsLease->Serial();
	TestTrue(TEXT("Nested subscriptions backing is granted"), VoxelGenerationAllocation::AllocatedBytes(Original.Dependencies) <= Original.DependenciesLease->Bytes());
	if (!Saturate()) return false;
	TestTrue(TEXT("Accepted cached closure needs no new grant under input pressure"), Coordinator.Ensure(
		MakeArrayView(&First, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer).bReady);
	const auto Blocked = Coordinator.Ensure(MakeArrayView(&Second, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer);
	TestTrue(TEXT("Metadata rejection preserves capacity wait"), !Blocked.bReady && !Blocked.bFailed);
	const auto& Retained = Coordinator.Storage->Consumers.FindChecked(Consumer);
	TestEqual(TEXT("Metadata rejection preserves old subscription"), Retained.Dependencies.FindChecked(First).Id, Id);
	TestEqual(TEXT("Metadata rejection retains old subscription backing"), Retained.DependenciesLease->Serial(), EdgeLease);
	TestEqual(TEXT("Metadata rejection retains old donation backing"), Coordinator.Storage->States.FindChecked(First).DonationsLease->Serial(), DonationLease);
	TestFalse(TEXT("Rejected metadata does not add the new donation"), Coordinator.Storage->States.FindChecked(Second).Donations.Contains(Consumer));
	Blocker.Reset();
	if (!Coordinator.Ensure(MakeArrayView(&Second, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer).bReady) return false;
	TestFalse(TEXT("Restored input admits replacement"), Coordinator.Storage->Consumers.FindChecked(Consumer).Dependencies.Contains(First));
	auto OldStorage = Coordinator.Storage;
	const uint64 OldIncarnation = Coordinator.ControlConsumer.OwnerIncarnation;
	Coordinator.Reset();
	TestTrue(TEXT("Reset disconnects the old callback owner"), OldStorage->Owner == nullptr && Coordinator.Storage == nullptr);
	TestTrue(TEXT("Old reader retains the owned backing"), OldStorage->StatesLease && OldStorage->Consumers.Num() == 2);
	TestTrue(TEXT("Next storage has a distinct resource incarnation"), Coordinator.ControlConsumer.OwnerIncarnation > OldIncarnation);
	if (!Coordinator.Ensure(MakeArrayView(&Second, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer).bReady) return false;
	TestTrue(TEXT("Old and new storage remain independent"), Coordinator.Storage != OldStorage && OldStorage->Owner == nullptr);
	OldStorage.Reset();
	Coordinator.ReleaseConsumer(Consumer);
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCoordinatorWakeLifetimeTest,
	"WHFramework.Voxel.FineRefactor.Generation.CoordinatorWakeLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCoordinatorWakeLifetimeTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelTaskScheduler Scheduler(true);
	FVoxelGenerationPlanDependency Fact;
	FVoxelHydrologyPlanPtr Plan;
	FString Error;
	if (!Cache->GetOrBuildHydrology(Fact.HydrologyKey,
		[](FVoxelHydrologyPlan&, FString& OutError) { OutError.Reset(); return true; }, Plan, Error)) return false;
	int32 Calls = 0;
	{
		FVoxelGenerationPlanCoordinator Coordinator(Scheduler, Config, Cache, 71);
		const auto First = FVoxelGenerationConsumerKey::Section(71, FGuid::NewGuid(), 1, {}, 1);
		const auto Second = FVoxelGenerationConsumerKey::Section(71, FGuid::NewGuid(), 1, {}, 1);
		if (!Coordinator.Ensure(MakeArrayView(&Fact, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, First).bReady ||
			!Coordinator.Ensure(MakeArrayView(&Fact, 1), EVoxelWorkClass::Visible, 1, 32.0, 0.0, Second).bReady) return false;
		Coordinator.SetConsumerWakeCallback([&](const FVoxelGenerationConsumerKey&) { ++Calls; });
		const auto Before = Scheduler.GetRuntimeResources()->Snapshot();
		Coordinator.WakeConsumers(Fact);
		Coordinator.WakeConsumers(Fact);
		const auto After = Scheduler.GetRuntimeResources()->Snapshot();
		for (int32 Pool = 0; Pool < int32(EVoxelTerrainResourcePool::Count); ++Pool)
			TestEqual(TEXT("Queued notification allocates no additional backing"), After.Used[Pool], Before.Used[Pool]);
		TestEqual(TEXT("Notification is deferred"), Calls, 0);
		TestEqual(TEXT("Expired frame deadline retains pending wakes"), Coordinator.AdvanceConsumerWakes(FPlatformTime::Seconds() - 1.0, 64), 0);
		const auto Head = Coordinator.Storage->WakeHead;
		const auto Removed = Head == First ? Second : First;
		Coordinator.SetConsumerWakeCallback([&](const FVoxelGenerationConsumerKey& Key)
		{
			++Calls;
			TestTrue(TEXT("Original complete consumer key is delivered"), Key == Head);
			Coordinator.ReleaseConsumer(Removed);
		});
		TestEqual(TEXT("Reentrant cancellation removes the other pending wake"), Coordinator.AdvanceConsumerWakes(FPlatformTime::Seconds() + 1.0, 64), 1);
		TestEqual(TEXT("Duplicate fact notifications coalesce per subscription"), Calls, 1);
		Coordinator.WakeConsumers(Fact);
		Coordinator.SetConsumerWakeCallback([&](const FVoxelGenerationConsumerKey&)
		{
			++Calls;
			Coordinator.Reset();
		});
		TestEqual(TEXT("Reset inside callback leaves no dangling queue references"), Coordinator.AdvanceConsumerWakes(FPlatformTime::Seconds() + 1.0, 64), 1);
		TestTrue(TEXT("Reset detaches the whole queue"), Coordinator.Storage == nullptr);
	}
	TSharedPtr<FVoxelGenerationPlanCoordinator::FStorage, ESPMode::ThreadSafe> Retired;
	{
		FVoxelGenerationPlanCoordinator Coordinator(Scheduler, Config, Cache, 73);
		Fact.HydrologyKey.Coordinate = FIntPoint(9, -10);
		const auto Consumer = FVoxelGenerationConsumerKey::Section(73, FGuid::NewGuid(), 1, {}, 1);
		Coordinator.Ensure(MakeArrayView(&Fact, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer);
		Retired = Coordinator.Storage;
		TestEqual(TEXT("Missing fact owns one queued production attempt"), Scheduler.ActiveCount(), 1);
	}
	TestTrue(TEXT("Destroyed coordinator invalidates even a pinned old storage owner"), Retired && Retired->Owner == nullptr);
	const double CancellationDeadline = FPlatformTime::Seconds() + 5.0;
	while (Scheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < CancellationDeadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::SleepNoStats(0.001f);
	}
	TestEqual(TEXT("Canceled callback can be consumed after coordinator destruction"), Scheduler.ActiveCount(), 0);
	Retired.Reset();
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPlanCaptureCapacityTest,
	"WHFramework.Voxel.FineRefactor.Generation.PlanCaptureCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPlanCaptureCapacityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelTaskScheduler Scheduler(true);
	FVoxelGenerationPlanCoordinator Coordinator(Scheduler, Config, Cache, 79);
	const auto Resources = Scheduler.GetRuntimeResources();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(79, FGuid::NewGuid(), 1, {}, 1);
	const FVoxelGenerationBounds Bounds{FIntVector(-16, 0, 0), FIntVector(0, 16, 16)};
	FVoxelGenerationPlanDependencyCapture Capture;
	FString Error;
	if (!TestTrue(TEXT("原采集入口接受带租约的完整闭包"), Coordinator.GatherForBounds(Bounds, Consumer, Capture, Error))) return false;
	TestTrue(TEXT("输出 backing 由原 Input 账本租约持有"), Capture.Lease && Capture.Values.GetAllocatedSize() <= Capture.Lease->Bytes());
	const auto* AcceptedData = Capture.Values.GetData();
	const int32 AcceptedCount = Capture.Values.Num();
	const uint64 AcceptedLease = Capture.Lease->Serial();
	FVoxelGenerationPlanKeyRanges Ranges;
	if (!FVoxelGenerationQuery::GetPlanKeyRanges(Bounds, Config->Recipe->Settings, Ranges, Error)) return false;
	TestEqual(TEXT("负坐标规划起点向负无穷取整"), Ranges.TileMin.X, -2);
	TestEqual(TEXT("半开上界在零处不加入原边界外瓦片"), Ranges.TileMax.X, 0);
	for (const auto& Fact : Capture.Values)
	{
		FVoxelGenerationPlanDependencyCapture Reads;
		if (!Coordinator.GatherPrerequisites(Fact, Reads, Error)) return false;
		for (const auto& Read : Reads.Values)
			TestTrue(TEXT("每个真实事实的全部读取位于公开捕获内"), Capture.Values.Contains(Read));
	}
	const auto CheckPreserved = [&]
	{
		TestTrue(TEXT("拒收保留同一已接受正文地址"), Capture.Values.GetData() == AcceptedData);
		TestEqual(TEXT("拒收保留原闭包数量"), Capture.Values.Num(), AcceptedCount);
		TestEqual(TEXT("拒收保留原 Input 租约"), Capture.Lease->Serial(), AcceptedLease);
	};
	const auto Blocker = FVoxelGenerationConsumerKey::Section(79, FGuid::NewGuid(), 1, {}, 1);
	for (const auto Pool : {EVoxelTerrainResourcePool::Working, EVoxelTerrainResourcePool::Input})
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Pressure;
		if (!Resources->Acquire(Blocker, Pool, 1, Pressure)) return false;
		const auto Snapshot = Resources->Snapshot();
		const uint64 Limit = Pool == EVoxelTerrainResourcePool::Working ? Snapshot.Limits.WorkingBytes : Snapshot.Limits.InputBytes;
		if (!Pressure->Resize(Pressure->Bytes() + Limit - Snapshot.Used[int32(Pool)])) return false;
		TestFalse(TEXT("真实容量压力拒收新捕获"), Coordinator.GatherForBounds(Bounds, Consumer, Capture, Error));
		TestTrue(TEXT("暂时拒收返回容量等待"), Error.StartsWith(TEXT("CapacityBlocked")));
		CheckPreserved();
		Pressure.Reset();
	}
	auto ForeignConsumer = Consumer;
	++ForeignConsumer.WorldEpoch;
	TestFalse(TEXT("另一世界不能向原 Coordinator 提交采集"), Coordinator.GatherForBounds(Bounds, ForeignConsumer, Capture, Error));
	CheckPreserved();
	const FVoxelGenerationBounds Huge{FIntVector(MIN_int32, MIN_int32, 0), FIntVector(MAX_int32, MAX_int32, 16)};
	TestFalse(TEXT("完整整数范围在 Keys 或 Closure 分配前拒收"), Coordinator.GatherForBounds(Huge, Consumer, Capture, Error));
	TestTrue(TEXT("非法容器规模不是可恢复容量等待"), Error.StartsWith(TEXT("Oversized")));
	CheckPreserved();
	const FVoxelGenerationBounds Edge{FIntVector(MAX_int32 - 16, 0, 0), FIntVector(MAX_int32, 16, 16)};
	TestTrue(TEXT("世界末端扩展使用宽整数并截到合法坐标"), FVoxelGenerationQuery::GetPlanKeyRanges(Edge, Config->Recipe->Settings, Ranges, Error));
	TestEqual(TEXT("世界末端保留真实最后瓦片"), Ranges.TileMax.X, MAX_int32 / FVoxelGenerationQuery::GenerationPlanTileSide);
	FVoxelGenerationPlanDependencyCapture Replacement;
	if (!Coordinator.GatherForBounds(Bounds, Consumer, Replacement, Error)) return false;
	const uint64 ReplacementLease = Replacement.Lease->Serial();
	const auto BeforeMove = Resources->Snapshot();
	Capture = MoveTemp(Replacement);
	TestEqual(TEXT("替换整体转交新正文的原租约"), Capture.Lease->Serial(), ReplacementLease);
	TestTrue(TEXT("替换源不再拥有正文或租约"), !Replacement.Lease && Replacement.Values.IsEmpty());
	TestTrue(TEXT("旧正文实际释放后才退还容量"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)] < BeforeMove.Used[int32(EVoxelTerrainResourcePool::Input)]);
	FVoxelGenerationPlanDependency Hydrology;
	TestTrue(TEXT("叶事实捕获为空且不分配正文"), Coordinator.GatherPrerequisites(Hydrology, Capture, Error));
	TestTrue(TEXT("空捕获撤销旧正文与原租约"), !Capture.Lease && Capture.Values.IsEmpty());
	Scheduler.StopAndJoin();
	return true;
}

#endif
