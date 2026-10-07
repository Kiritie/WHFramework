#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelDependencyPriorityCapacityTest,
	"WHFramework.Voxel.FineRefactor.Generation.DependencyPriorityCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelDependencyPriorityCapacityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(59, FGuid::NewGuid(), 1, {}, 1);
	FVoxelTerrainDependencyPriority Graph(Resources.ToSharedRef(), Consumer);
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
	const uint64 Backing = Graph.Storage->Nodes.GetAllocatedSize();
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
	FVoxelTaskScheduler Scheduler;
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
	const uint64 Subscription = Coordinator.ConsumerDependencies.FindChecked(Consumer).FindChecked(First);
	const FGuid ConsumerNode = Coordinator.ConsumerPriorityNodes.FindChecked(Consumer);
	const auto Resources = Scheduler.GetRuntimeResources();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WorkingBlocker;
	const auto BeforeWorking = Resources->Snapshot();
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
		BeforeWorking.Limits.WorkingBytes - BeforeWorking.Used[uint8(EVoxelTerrainResourcePool::Working)], WorkingBlocker)) return false;
	FVoxelGenerationPlanDependency Cold;
	Cold.Kind = EVoxelGenerationPlanKind::Structure;
	Cold.TileKey.Coordinate = FIntVector(400, -30, 0);
	const int32 BeforeColdStates = Coordinator.States.Num();
	const int32 BeforeColdFacts = Coordinator.PriorityFacts.Num();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const auto ColdStatus = Coordinator.Ensure(MakeArrayView(&Cold, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer);
		TestTrue(TEXT("Cold closure is suspended by actual Working lease pressure"), !ColdStatus.bReady && !ColdStatus.bFailed && ColdStatus.Error.StartsWith(TEXT("CapacityBlocked")));
		TestEqual(TEXT("Failed closure rolls back unowned fact states"), Coordinator.States.Num(), BeforeColdStates);
		TestEqual(TEXT("Repeated rejection leaves no orphan priority facts"), Coordinator.PriorityFacts.Num(), BeforeColdFacts);
	}
	const auto Blocked = Coordinator.Ensure(MakeArrayView(&Second, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer);
	TestFalse(TEXT("Graph grant failure is not Ready"), Blocked.bReady);
	TestFalse(TEXT("Graph grant failure is not terminal fact failure"), Blocked.bFailed);
	TestTrue(TEXT("Graph grant failure exposes capacity reason"), Blocked.Error.StartsWith(TEXT("CapacityBlocked")));
	TestEqual(TEXT("Failed topology retains old subscription"), Coordinator.ConsumerDependencies.FindChecked(Consumer).FindChecked(First), Subscription);
	TestEqual(TEXT("Failed topology retains original consumer node"), Coordinator.ConsumerPriorityNodes.FindChecked(Consumer), ConsumerNode);
	TestFalse(TEXT("Rejected first subscription removes its newly registered fact"), Coordinator.States.Contains(Second));
	WorkingBlocker.Reset();
	if (!TestTrue(TEXT("Original consumer resumes new requested topology"), Coordinator.Ensure(
		MakeArrayView(&Second, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer).bReady)) return false;
	TestFalse(TEXT("Accepted topology withdraws old subscription"), Coordinator.ConsumerDependencies.FindChecked(Consumer).Contains(First));
	TestTrue(TEXT("Accepted topology records new subscription"), Coordinator.ConsumerDependencies.FindChecked(Consumer).Contains(Second));
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
	TestTrue(TEXT("Waiting consumer has only its original control node"), Coordinator.ConsumerPriorityNodes.Contains(Denied));
	TestFalse(TEXT("Denied consumer has no accepted subscription"), Coordinator.ConsumerDependencies.Contains(Denied));
	Coordinator.ReleaseConsumer(Denied);
	TestFalse(TEXT("Cancellation removes control node even before first subscription"), Coordinator.ConsumerPriorityNodes.Contains(Denied));
	WorkingBlocker.Reset();
	Coordinator.States.FindChecked(Second).Prerequisites.Add(Second);
	const auto CycleStatus = Coordinator.Ensure(MakeArrayView(&Second, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer);
	TestTrue(TEXT("Stored read cycle is rejected before recursive re-registration"), CycleStatus.bFailed && CycleStatus.Error.Contains(TEXT("循环")));
	TestFalse(TEXT("Rejected cycle clears its registration marker"), Coordinator.States.FindChecked(Second).bRegistering);
	TestTrue(TEXT("Rejected read cycle retains accepted subscription"), Coordinator.ConsumerDependencies.FindChecked(Consumer).Contains(Second));
	Coordinator.States.FindChecked(Second).Prerequisites.Reset();
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
	const uint64 LeafSubscription = Coordinator.ConsumerDependencies.FindChecked(Consumer).FindChecked(Second);
	Coordinator.States.FindChecked(Middle).Prerequisites.Add(Second);
	Coordinator.States.FindChecked(Outer).Prerequisites.Add(Middle);
	if (!Coordinator.Ensure(MakeArrayView(&Outer, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Consumer).bReady) return false;
	const auto& Accepted = Coordinator.ConsumerDependencies.FindChecked(Consumer);
	TestTrue(TEXT("Consumer subscribes the dynamically extended middle fact"), Accepted.Contains(Middle));
	TestEqual(TEXT("Transitive replacement retains original leaf subscription identity"), Accepted.FindRef(Second), LeafSubscription);
	for (const auto& Prerequisite : Coordinator.States.FindChecked(Middle).Prerequisites)
		TestTrue(TEXT("Consumer retains every transitive leaf subscription"), Accepted.Contains(Prerequisite));
	Coordinator.ReleaseConsumer(Consumer);
	Coordinator.ReleaseConsumer(MiddleConsumer);
	Coordinator.ReleaseConsumer(OuterConsumer);
	for (const auto& Other : OtherConsumers) Coordinator.ReleaseConsumer(Other);
	Coordinator.Reset();
	Scheduler.StopAndJoin();
	return true;
}

#endif
