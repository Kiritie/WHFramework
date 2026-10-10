#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Rendering/VoxelCoverageIndex.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Runtime/VoxelChangeHierarchy.h"

namespace VoxelCoveragePersistentTest
{
	FVoxelCoverageRecord Record(const FIntVector& Coordinate, uint64 Revision)
	{
		FVoxelCoverageRecord Result;
		Result.Geometry = {{EVoxelTerrainRepresentation::Fine, Coordinate, 0}, 1, -256, 256, 17, 23};
		Result.Geometry.Key.TryBounds(-256, 256, Result.Min, Result.Max);
		Result.Stamp.ContentRevision = Revision;
		Result.bReady = true;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCoverageFrozenContinuationTest,
	"WHFramework.Voxel.FineRefactor.Coverage.FrozenContinuation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCoverageFrozenContinuationTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelCoverageIndex Index;
	FString Error;
	const auto First = VoxelCoveragePersistentTest::Record(FIntVector(-1, 0, -1), 1);
	if (!TestTrue(TEXT("初始独立三维记录"), Index.Upsert(First, Error))) return false;
	const FVoxelCoverageIndex Frozen = Index;
	FVoxelCoverageLocalSnapshot Snapshot;
	auto Capture = Index.CaptureSteps(First.Min, First.Max, Snapshot);
	// 首次 Resume 之前已修改 Current；捕获仍须使用调用时的根。
	const auto Second = VoxelCoveragePersistentTest::Record(First.Geometry.Key.Coordinate, 2);
	if (!TestTrue(TEXT("局部版本更新"), Index.Upsert(Second, Error))) return false;
	for (int32 Step = 0; Step < 8192 && !Capture.IsComplete(); ++Step) Capture.Resume();
	if (!TestTrue(TEXT("分帧捕获完整结束"), Capture.IsComplete())) return false;
	if (!TestEqual(TEXT("固定根只返回一个原记录"), Snapshot.Records.Num(), 1)) return false;
	TestEqual(TEXT("捕获返回原内容版本"), Snapshot.Records[0]->Stamp.ContentRevision, uint64(1));
	TestEqual(TEXT("原根仍可独立查询"), Frozen.Find(First.Geometry.Key)->Stamp.ContentRevision, uint64(1));
	TestFalse(TEXT("原守卫不能作为当前版本提交"), Index.IsCurrent(Snapshot));
	TestTrue(TEXT("原根接受自身读守卫"), Frozen.IsCurrent(Snapshot));
	TestTrue(TEXT("当前记录可退休"), Index.Remove(First.Geometry.Key));
	TestEqual(TEXT("空空间路径立即退休"), Index.NumIndexNodes(), 0);
	TestTrue(TEXT("退休不破坏旧冻结根"), Frozen.Find(First.Geometry.Key).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCoverageChangedPathCapacityTest,
	"WHFramework.Voxel.FineRefactor.Coverage.ChangedPathCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCoverageChangedPathCapacityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	FVoxelTerrainResourceLimits Limits;
	if (!Resources->Configure(Limits)) return false;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(31, FGuid::NewGuid(), 1, {}, 1);
	FVoxelCoverageIndex Index;
	Index.BindResources(Resources, Consumer);
	FString Error;
	const auto First = VoxelCoveragePersistentTest::Record({}, 1);
	if (!TestTrue(TEXT("授予后写入初始路径"), Index.Upsert(First, Error))) return false;
	const auto Frozen = Index;
	const auto Before = Resources->Snapshot();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
		Limits.InputBytes - Before.Used[int32(EVoxelTerrainResourcePool::Input)] - 4096, Blocker)) return false;
	const auto Second = VoxelCoveragePersistentTest::Record({}, 2);
	TestFalse(TEXT("没有新旧路径峰值容量时不修改当前根"), Index.Upsert(Second, Error));
	TestTrue(TEXT("容量阻塞明确可重试"), Error.StartsWith(TEXT("CapacityBlocked")));
	TestEqual(TEXT("阻塞保留原版本"), Index.Find(First.Geometry.Key)->Stamp.ContentRevision, uint64(1));
	Blocker.Reset();
	if (!TestTrue(TEXT("释放容量后同一局部更新完成"), Index.Upsert(Second, Error))) return false;
	TestEqual(TEXT("当前根接纳新版本"), Index.Find(First.Geometry.Key)->Stamp.ContentRevision, uint64(2));
	TestEqual(TEXT("冻结根保持原版本"), Frozen.Find(First.Geometry.Key)->Stamp.ContentRevision, uint64(1));
	TestTrue(TEXT("真实存活路径仍持有输入租约"), Resources->Snapshot().LeaseCount > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineFeedbackFrozenVersionsTest,
	"WHFramework.Voxel.FineRefactor.Coverage.FrozenContentVersions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineFeedbackFrozenVersionsTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelChangeHierarchy Hierarchy;
	const FVoxelFineShardKey Shard{FIntVector(-32, 0, 0), 32};
	TestTrue(TEXT("注册负坐标 Shard"), Hierarchy.RegisterFineShard(Shard));
	const FIntVector Inside(-2, 0, 0), Halo(0, 0, 0), ForeignZ(-2, 0, 6);
	const uint64 First = Hierarchy.InvalidateSection(Inside);
	const auto Frozen = Hierarchy.CaptureVersionSnapshot();
	const uint64 Second = Hierarchy.InvalidateSection(Halo);
	const auto Current = Hierarchy.CaptureVersionSnapshot();
	TestEqual(TEXT("原 Section 版本固定"), Frozen.GetSectionRevision(Inside), First);
	TestEqual(TEXT("捕获后的编辑不进入原切点"), Frozen.GetSectionRevision(Halo), uint64(0));
	TestEqual(TEXT("原 Shard 版本固定"), Frozen.GetFineShardRevision(Shard), First);
	TestEqual(TEXT("新切点包括精细相邻 halo 编辑"), Current.GetFineShardRevision(Shard), Second);
	TestFalse(TEXT("实际依赖范围拒绝旧切点"), Hierarchy.IsVersionSnapshotCurrent(Frozen, Shard.Bounds()));
	Hierarchy.InvalidateSection(ForeignZ);
	TestEqual(TEXT("不同 Z 层编辑不污染局部 Shard"), Hierarchy.CaptureVersionSnapshot().GetFineShardRevision(Shard), Second);
	TestTrue(TEXT("无关内容变化不否决 Fine 候选"), Hierarchy.IsVersionSnapshotCurrent(Current, Shard.Bounds()));
	Hierarchy.Reset();
	TestFalse(TEXT("世界换代拒绝旧范围证明"), Hierarchy.IsVersionSnapshotCurrent(Current, Shard.Bounds()));
	TestEqual(TEXT("世界释放后原捕获仍保活"), Frozen.GetFineShardRevision(Shard), First);
	TestEqual(TEXT("新世界不继承原 Section 版本"), Hierarchy.CaptureVersionSnapshot().GetSectionRevision(Inside), uint64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCoverageChangedRangeTest,
	"WHFramework.Voxel.FineRefactor.Coverage.ChangedRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCoverageChangedRangeTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelCoverageIndex Index;
	FString Error;
	auto Local = VoxelCoveragePersistentTest::Record(FIntVector(-1, 0, -1), 1);
	auto Far = VoxelCoveragePersistentTest::Record(FIntVector(-1024, 0, -1), 1);
	if (!Index.Upsert(Local, Error) || !Index.Upsert(Far, Error)) return false;
	const auto Original = Index;
	Far.Stamp.ContentRevision = 2;
	if (!Index.Upsert(Far, Error)) return false;
	TestTrue(TEXT("同半空间内无关编辑不污染局部证明"), Index.IsRangeCurrent(Original, Local.Min, Local.Max));
	TestFalse(TEXT("命中实际变化路径拒绝原证明"), Index.IsRangeCurrent(Original, Far.Min, Far.Max));
	const auto BeforePresentation = Index;
	Local.bCommitted = true;
	if (!Index.Upsert(Local, Error)) return false;
	TestTrue(TEXT("呈现变化不改变内容/拓扑范围"), Index.IsRangeCurrent(BeforePresentation, Local.Min, Local.Max));
	TestFalse(TEXT("呈现守卫独立拒绝旧范围"), Index.IsRangeCurrent(BeforePresentation, Local.Min, Local.Max, true));
	Index.Remove(Local.Geometry.Key);
	const auto Gap = Index;
	if (!Index.Upsert(Local, Error)) return false;
	Index.Remove(Local.Geometry.Key);
	TestFalse(TEXT("空 gap 的插入/删除 ABA 仍被拒绝"), Index.IsRangeCurrent(Gap, Local.Min, Local.Max));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCoverageIdentityIsolationTest,
	"WHFramework.Voxel.FineRefactor.Coverage.IdentityIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCoverageIdentityIsolationTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FString Error;
	const auto Original = VoxelCoveragePersistentTest::Record({}, 1);
	FVoxelCoverageIndex First;
	FVoxelCoverageIndex Independent;
	if (!First.Upsert(Original, Error) || !Independent.Upsert(Original, Error)) return false;
	FVoxelCoverageLocalSnapshot Guard;
	First.Capture(Original.Min, Original.Max, Guard);
	TestTrue(TEXT("原索引接受自身证明"), First.IsCurrent(Guard));
	TestFalse(TEXT("独立索引相同内容与序号仍拒绝外来证明"), Independent.IsCurrent(Guard));
	TestFalse(TEXT("独立索引不能复用外来的范围证明"), Independent.IsRangeCurrent(First, Original.Min, Original.Max));
	const auto Frozen = First;
	auto Fork = Frozen;
	auto Left = Original;
	Left.Stamp.ContentRevision = 2;
	auto Right = Original;
	Right.Stamp.ContentRevision = 3;
	if (!First.Upsert(Left, Error) || !Fork.Upsert(Right, Error)) return false;
	TestFalse(TEXT("同一祖先的两个修改分支不能碰撞版本"), First.IsRangeCurrent(Fork, Original.Min, Original.Max));
	TestTrue(TEXT("冻结祖先仍接受自身范围"), Frozen.IsRangeCurrent(Frozen, Original.Min, Original.Max));
	FVoxelCoverageLocalSnapshot Changed;
	auto Capture = Independent.CaptureChangedSteps(First, Changed);
	for (int32 Step = 0; Step < 8192 && !Capture.IsComplete(); ++Step) Capture.Resume();
	TestTrue(TEXT("独立根的变化捕获完成"), Capture.IsComplete());
	TestEqual(TEXT("独立根首次捕获保留全部记录"), Changed.Records.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCoverageCapacityRetryTest,
	"WHFramework.Voxel.FineRefactor.Coverage.CapacityRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCoverageCapacityRetryTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	FVoxelTerrainResourceLimits Limits;
	if (!Resources->Configure(Limits)) return false;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(31, FGuid::NewGuid(), 1, {}, 1);
	FVoxelCoverageIndex Index;
	Index.BindResources(Resources, Consumer);
	FString Error;
	auto Record = VoxelCoveragePersistentTest::Record({}, 1);
	if (!Index.Upsert(Record, Error)) return false;
	const auto Before = Resources->Snapshot();
	const auto External = FVoxelGenerationConsumerKey::Section(31, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!Resources->Acquire(External, EVoxelTerrainResourcePool::Input,
		Limits.InputBytes - Before.Used[int32(EVoxelTerrainResourcePool::Input)] - 256, Blocker)) return false;
	for (const bool bFull : {false, true})
	{
		FVoxelCoverageLocalSnapshot Snapshot;
		auto Capture = bFull ? Index.CaptureAllSteps(Snapshot, true, false) :
			Index.CaptureSteps(Record.Min, Record.Max, Snapshot, true, false);
		for (int32 Step = 0; Step < 8192 && !Capture.IsComplete(); ++Step) Capture.Resume();
		TestTrue(TEXT("容量不足的捕获有限结束，不持有发布门禁等待"), Capture.IsComplete());
		TestTrue(TEXT("失败保留明确的容量状态，不能提交为空覆盖"), Snapshot.bCountOnly &&
			Snapshot.ResourceAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked && !Index.IsCurrent(Snapshot));
		TestTrue(TEXT("失败不保留输出数组租约"), !Snapshot.ResourceLease);
	}
	Blocker.Reset();
	Record.Stamp.ContentRevision = 2;
	if (!Index.Upsert(Record, Error)) return false;
	FVoxelCoverageLocalSnapshot Current;
	auto Retry = Index.CaptureAllSteps(Current, true, false);
	for (int32 Step = 0; Step < 8192 && !Retry.IsComplete(); ++Step) Retry.Resume();
	if (!TestTrue(TEXT("释放容量后最新根捕获成功"), Retry.IsComplete() && Index.IsCurrent(Current) && Current.Records.Num() == 1)) return false;
	TestEqual(TEXT("重试读取门禁释放期间接纳的最新版本"), Current.Records[0]->Stamp.ContentRevision, uint64(2));
	return true;
}

#endif
