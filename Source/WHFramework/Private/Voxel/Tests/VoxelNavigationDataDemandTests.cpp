#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
#include "Voxel/Save/VoxelRegionStore.h"
#include "Voxel/Streaming/VoxelEmergeManager.h"
#include "Voxel/Streaming/VoxelResidencyManager.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNavigationDataDemandTest,
	"WHFramework.Voxel.Navigation.IndependentDataDemandLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNavigationDataDemandTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	const auto Generator = MakeShared<FVoxelGenerationPipeline, ESPMode::ThreadSafe>(Config, Cache);
	const auto Registry = VoxelTest::MakeRegistry();
	FVoxelTaskScheduler Scheduler;
	FVoxelWorldRuntime Runtime(57, true, Registry, Generator);
	Runtime.BindTerrainResources(Scheduler.GetRuntimeResources());
	FVoxelRegionStore Store;
	FVoxelWorldManifest Manifest;
	FVoxelEmergeManager Emerge(Runtime, Scheduler, Generator, Config, Cache, Store, Manifest, Registry);
	FVoxelResidencyManager Residency(Runtime, Scheduler, {});
	Residency.SetEvictGraceFrames(0);
	const FIntVector Above(1, 0, 3);
	FVoxelRuntimeSectionPin First;
	FVoxelRuntimeSectionPin Second;
	const auto Resources = Scheduler.GetTerrainResources();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CapacityBlocker;
	const auto CapacityConsumer = FVoxelGenerationConsumerKey::Section(Runtime.Epoch(), FGuid::NewGuid(), 1, Above, 1);
	if (!TestTrue(TEXT("实际Input池可阻塞消费者控制表授予"), Resources->Acquire(CapacityConsumer,
		EVoxelTerrainResourcePool::Input, 1, CapacityBlocker))) return false;
	const auto ResourceState = Resources->Snapshot();
	if (!TestTrue(TEXT("句柄和账本已授予后填满实际Input池"), CapacityBlocker->Resize(1 + ResourceState.Limits.InputBytes -
		ResourceState.Used[int32(EVoxelTerrainResourcePool::Input)]))) return false;
	TestFalse(TEXT("容量失败在表分配和Runtime接纳之前返回"), Emerge.AcquireDataDemand(Above, First));
	TestNull(TEXT("容量失败不隐式申请Runtime或取得pin"), Runtime.FindSection(Above));
	TestFalse(TEXT("失败不制造有效访问句柄"), First.ConsumerId.IsValid());
	CapacityBlocker.Reset();
	if (!TestTrue(TEXT("独立Nav消费者可在无Source和无视觉Exact时取得实际Runtime"), Emerge.AcquireDataDemand(Above, First))) return false;
	if (!TestTrue(TEXT("重叠Nav消费者复用相同稳定实例"), Emerge.AcquireDataDemand(Above, Second))) return false;
	TestTrue(TEXT("共享消费者保留同一完整World/Incarnation"), First.Resource == Second.Resource);
	TestTrue(TEXT("同一资源实例的每次接纳有独立访问句柄"), First.ConsumerId != Second.ConsumerId);
	TestEqual(TEXT("每个消费者取得自己的真实pin"), Runtime.FindSection(Above)->GetPinCount(), 2);
	FVoxelInterestSet EmptyInterest;
	const double Deadline = FPlatformTime::Seconds() + 20.0;
	int32 Frame = 1;
	while (Runtime.FindSection(Above)->Status != EVoxelSectionStatus::DataReady && FPlatformTime::Seconds() < Deadline)
	{
		// Priority 重建不能取消独立 Nav 的真实生成/Overlay/Facts 依赖。
		++EmptyInterest.PriorityRevision;
		Emerge.Tick(EmptyInterest, uint64(Frame++), FPlatformTime::Seconds(), {}, 2, 1.0);
		Scheduler.Tick([&Emerge](FVoxelTaskResult&& Result) { Emerge.OnTask(MoveTemp(Result)); }, 2.0);
		Residency.Tick(EmptyInterest.Exact, uint64(Frame), FPlatformTime::Seconds());
		FPlatformProcess::Sleep(0.001f);
	}
	if (!TestTrue(TEXT("实际Scheduler和原生成/Overlay主链物化无Source数据"),
		Runtime.FindSection(Above)->Status == EVoxelSectionStatus::DataReady)) return false;
	TestTrue(TEXT("独立输入不向权威Interest加入视觉或Simulation需求"),
		EmptyInterest.Exact.IsEmpty() && EmptyInterest.Sources.IsEmpty() && !Runtime.FindSection(Above)->bSimulationWanted);
	FVoxelRuntimeSectionPin Stale = First;
	++Stale.Resource.Incarnation;
	TestFalse(TEXT("旧实例释放不能撤销新实例消费者"), Emerge.ReleaseDataDemand(Above, Stale));
	TestEqual(TEXT("拒绝旧实例释放后完整pin仍保留"), Runtime.FindSection(Above)->GetPinCount(), 2);
	TestTrue(TEXT("第一个消费者可以释放自己的实例"), Emerge.ReleaseDataDemand(Above, First));
	Residency.RequestRetirement(Above);
	Residency.Tick(EmptyInterest.Exact, uint64(Frame), FPlatformTime::Seconds());
	TestNotNull(TEXT("撤销一个Nav消费者不能回收另一消费者的上层输入"), Runtime.FindSection(Above));
	TestEqual(TEXT("撤销仅归还自己的pin"), Runtime.FindSection(Above)->GetPinCount(), 1);
	TestFalse(TEXT("同一实例上的重复旧释放不能归还剩余消费者pin"), Emerge.ReleaseDataDemand(Above, First));
	TestEqual(TEXT("独立句柄挡住同实例ABA释放"), Runtime.FindSection(Above)->GetPinCount(), 1);
	TestTrue(TEXT("最后消费者可以释放自己的实例"), Emerge.ReleaseDataDemand(Above, Second));
	const double RetirementDeadline = FPlatformTime::Seconds() + 2.0;
	while (Runtime.FindSection(Above) && FPlatformTime::Seconds() < RetirementDeadline)
	{
		Scheduler.Tick([&Emerge](FVoxelTaskResult&& Result) { Emerge.OnTask(MoveTemp(Result)); }, 2.0);
		Residency.Tick(EmptyInterest.Exact, uint64(Frame++), FPlatformTime::Seconds());
		FPlatformProcess::Sleep(0.001f);
	}
	TestNull(TEXT("最后Nav消费者撤销后真实退休事件可释放Runtime"), Runtime.FindSection(Above));
	Scheduler.StopAndJoin();
	return true;
}

#endif
