#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Voxel/Generation/VoxelGenerationPlanCoordinator.h"
#include "Voxel/Generation/VoxelGenerationPlanningRead.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelLocalPlanClosureTest, "WHFramework.Voxel.FineRefactor.Generation.LocalPlanClosure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelLocalPlanClosureTest::RunTest(const FString& InParameters)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelTaskScheduler Scheduler;
	FVoxelGenerationPlanCoordinator Coordinator(Scheduler, Config, Cache, 17);
	FVoxelGenerationPlanDependency Near, Far;
	Near.Kind = Far.Kind = EVoxelGenerationPlanKind::Structure;
	Far.TileKey.Coordinate = FIntVector(400, 0, 0);
	TArray<FVoxelGenerationPlanDependency> NearReads, FarReads;
	FString Error;
	TestTrue(TEXT("近处事实具有独立闭包"), Coordinator.GatherPrerequisites(Near, NearReads, Error));
	TestTrue(TEXT("远处事实具有独立闭包"), Coordinator.GatherPrerequisites(Far, FarReads, Error));
	TestEqual(TEXT("结构只采样所属瓦片水文"), NearReads.Num(), 1);
	TestFalse(TEXT("远处闭包不别名近处水文"), NearReads.Contains(FarReads[0]));
	for (const auto& Read : NearReads)
	{
		FVoxelHydrologyPlanPtr Plan;
		TestTrue(TEXT("预置近处已完成的事实"), Cache->GetOrBuildHydrology(Read.HydrologyKey,
			[](FVoxelHydrologyPlan&, FString& OutError) { OutError.Reset(); return true; }, Plan, Error));
	}
	const auto Consumer = FVoxelGenerationConsumerKey::Section(17, FGuid::NewGuid(), 3, {}, 1);
	const FVoxelGenerationPlanDependency Facts[] = {Near, Far};
	const auto Status = Coordinator.Ensure(MakeArrayView(Facts), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer);
	TestFalse(TEXT("调用者仍等待完整请求结果"), Status.bReady);
	TestTrue(TEXT("近处计划可在远处水文未完成时排队"), Coordinator.States.FindChecked(Near).bInFlight);
	TestFalse(TEXT("远处计划等待自己的水文"), Coordinator.States.FindChecked(Far).bInFlight);
	TestTrue(TEXT("远处水文拥有独立任务"), Coordinator.States.FindChecked(FarReads[0]).bInFlight);
	const auto* Donated = Coordinator.PriorityGraph.Find(Coordinator.States.FindChecked(FarReads[0]).PriorityNode);
	TestTrue(TEXT("生产依赖图将完整 Source 优先级送至事实叶子"), Donated && Donated->SourceStableKey == Consumer.OwnerId &&
		Donated->Band == EVoxelTerrainServiceBand::P0);
	Coordinator.ReleaseConsumer(Consumer);
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelEnvironmentAttemptIsolationTest, "WHFramework.Voxel.FineRefactor.Generation.EnvironmentAttemptIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEnvironmentAttemptIsolationTest::RunTest(const FString& InParameters)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	FVoxelTaskScheduler Scheduler;
	FVoxelGenerationPlanCoordinator Coordinator(Scheduler, Config, MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>(), 19);
	const FVoxelGenerationBounds Column{FIntVector(-1, 8, 0), FIntVector(-1, 8, 0)};
	const auto First = Coordinator.EnvironmentConsumer(Column, true);
	TestTrue(TEXT("环境重复请求保持同一在途身份"), First == Coordinator.EnvironmentConsumer(Column, true));
	FVoxelGenerationPlanDependency Fact;
	Coordinator.Ensure(MakeArrayView(&Fact, 1), EVoxelWorkClass::Interactive, 0, 0.0, 0.0, First);
	Coordinator.ReleaseEnvironment(FIntPoint(-1, 8));
	const auto Second = Coordinator.EnvironmentConsumer(Column, true);
	TestTrue(TEXT("释放后新请求取得严格递增的尝试代"), Second.RequestSerial > First.RequestSerial);
	TestFalse(TEXT("旧代已从完整订阅表移除"), Coordinator.ConsumerDependencies.Contains(First));
	TestFalse(TEXT("新代不能被旧代图节点复用"), Coordinator.ConsumerPriorityNodes.Contains(First));
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
		FVoxelGenerationPlanningReadScope Scope([&](EVoxelGenerationPlanKind Kind, const FIntVector& Coordinate)
		{
			++OuterReads;
			Actual.Kind = Kind;
			Actual.TileKey.Coordinate = Coordinate;
		});
		FVoxelGenerationQuery Query;
		TestTrue(TEXT("普通 Query 构造受 Worker 观察范围约束"), FVoxelGenerationQuery::Create(Config, Cache, Query, Error));
		FVoxelCavePlanPtr Cave;
		TestFalse(TEXT("缺失洞穴不能在项目 Worker 内同步构建"), Query.EnsureCavePlan({FIntVector(7, -3, 2)}, Cave, Error));
		TestTrue(TEXT("返回可等待的实际依赖键"), Error.StartsWith(TEXT("DependencyNotReady")) &&
			Actual.Kind == EVoxelGenerationPlanKind::Cave && Actual.Coordinate() == FIntVector(7, -3, 2));
		TestFalse(TEXT("Cache 中没有隐式生成的洞穴事实"), Cache->FindCave({FIntVector(7, -3, 2)}, Cave));
		{
			FVoxelGenerationPlanningReadScope Nested([&](EVoxelGenerationPlanKind, const FIntVector&) { ++InnerReads; });
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
		FVoxelGenerationPlanningReadScope Outer({}, &Stamp, &Cancel);
		{
			FVoxelGenerationPlanningReadScope Inner({});
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
#endif
