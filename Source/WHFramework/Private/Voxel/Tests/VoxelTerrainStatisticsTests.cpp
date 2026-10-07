#include "Misc/AutomationTest.h"
#include "Voxel/Rendering/VoxelTerrainStatistics.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainStatisticsWindowTest,
	"WHFramework.Voxel.Terrain.Statistics.BoundedWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainStatisticsWindowTest::RunTest(const FString& InParameters)
{
	auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, FIntVector::ZeroValue, 1);
	FVoxelTerrainStatistics Statistics;
	TestTrue(TEXT("固定容量预授予"), Statistics.Initialize(Resources, Consumer, 2, 2));
	FVoxelTerrainMetricEvidence First;
	First.Target = {1, FIntVector::ZeroValue, FIntVector(32), EVoxelTerrainMetricRepresentation::F1};
	First.Representation = EVoxelTerrainMetricRepresentation::F1;
	First.OwnershipAtoms = First.PresentedOwnershipAtoms = 8;
	First.bWanted = First.bPresented = true;
	TestTrue(TEXT("旧提交仍可用，新版尚未完成"), Statistics.SetEvidence(First));
	auto Counters = Statistics.Get(EVoxelTerrainMetricRepresentation::F1, 0);
	TestEqual(TEXT("旧版本真实覆盖仍在"), Counters.PresentedAtoms, int64(8));
	TestEqual(TEXT("新版进度独立"), Counters.LatestPresented, int64(0));
	auto Second = First;
	Second.Target.Min.Z = 32;
	Second.Target.Max.Z = 64;
	TestTrue(TEXT("不同Z完整键独立"), Statistics.SetEvidence(Second));
	auto Third = First;
	Third.Target.Min.Z = 64;
	Third.Target.Max.Z = 96;
	TestFalse(TEXT("容量不足不扩表"), Statistics.SetEvidence(Third));
	Statistics.RemoveEvidence(First.Target);
	TestTrue(TEXT("退休后恢复容量"), Statistics.SetEvidence(Third));

	const FGuid A = FGuid::NewGuid(), B = FGuid::NewGuid(), C = FGuid::NewGuid();
	TestTrue(TEXT("首次完成"), Statistics.CompleteWork(A, 3.0));
	TestFalse(TEXT("相同Attempt不重复完成"), Statistics.CompleteWork(A, 7.0));
	TestTrue(TEXT("第二个未结算完成"), Statistics.CompleteWork(B, 2.0));
	TestFalse(TEXT("未结算记录不能因窗口覆盖丢失"), Statistics.CompleteWork(C, 1.0));
	Statistics.ResolveWork(A, true);
	Statistics.ResolveWork(A, true);
	TestTrue(TEXT("只回收已结算的旧身份"), Statistics.CompleteWork(C, 1.0));
	Statistics.ResolveWork(B, false);
	Statistics.ResolveWork(C, true);
	const auto Work = Statistics.GetWorkAccounting();
	TestEqual(TEXT("Completed只登记一次"), Work.Completed, uint64(3));
	TestEqual(TEXT("Used只结算一次"), Work.UsedMilliseconds, 4.0);
	TestEqual(TEXT("Discarded包含真实耗时"), Work.DiscardedMilliseconds, 2.0);
	TestEqual(TEXT("全部输出最终结算"), Work.Unresolved, uint64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainStatisticsReceiptTest,
	"WHFramework.Voxel.Terrain.Statistics.ReceiptLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainStatisticsReceiptTest::RunTest(const FString& InParameters)
{
	auto Statistics = MakeShared<FVoxelTerrainStatistics, ESPMode::ThreadSafe>();
	{
		auto Receipt = MakeShared<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe>(Statistics, nullptr);
		TestTrue(TEXT("注册完整执行身份"), Receipt->IsRegistered());
		TestTrue(TEXT("continuation累计时间完成一次"), Receipt->Complete(9.0));
		TestFalse(TEXT("多个消费者不重复完成"), Receipt->Complete(9.0));
		Receipt->Resolve(true);
		Receipt->Resolve(false);
	}
	{
		auto Receipt = MakeShared<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe>(Statistics, nullptr);
		Receipt->Complete(5.0);
		// 没有任何最终消费者接纳；最后一个强引用退休即 Discarded。
	}
	const auto Work = Statistics->GetWorkAccounting();
	TestEqual(TEXT("接纳耗时"), Work.UsedMilliseconds, 9.0);
	TestEqual(TEXT("退休耗时"), Work.DiscardedMilliseconds, 5.0);
	TestEqual(TEXT("退休后没有悬空结算"), Work.Unresolved, uint64(0));
	Statistics->AddFrame(14.0, EVoxelTerrainFrameMetric::GameThread);
	Statistics->AddFrame(3.0, EVoxelTerrainFrameMetric::TerrainGameThread);
	Statistics->AddFrame(std::numeric_limits<double>::quiet_NaN(), EVoxelTerrainFrameMetric::GPU);
	TestEqual(TEXT("GT独立分位"), Statistics->CaptureFrameQuantiles().P95, 14.0);
	TestEqual(TEXT("Terrain独立分位"), Statistics->CaptureFrameQuantiles(EVoxelTerrainFrameMetric::TerrainGameThread).P95, 3.0);
	TestEqual(TEXT("未知GPU不伪装为零耗时"), Statistics->CaptureFrameQuantiles(EVoxelTerrainFrameMetric::GPU).Samples, 0);
	return true;
}
#endif
