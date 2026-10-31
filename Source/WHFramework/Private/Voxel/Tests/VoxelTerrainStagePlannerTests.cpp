#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Rendering/VoxelTerrainStagePlanner.h"
#include "Voxel/Rendering/VoxelCoverageIndex.h"
#include "Voxel/Rendering/VoxelFinePage.h"
#include "Voxel/Streaming/VoxelTerrainDomainPlanner.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainScreenErrorSymmetryTest, "WHFramework.Voxel.Rendering.Plan.ScreenErrorSymmetry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainScreenErrorSymmetryTest::RunTest(const FString& InParameters)
{
	FVoxelTerrainStageSource Left;
	Left.Source.Center = FIntVector(-48, 0, 0);
	Left.Source.VerticalFovDegrees = 90.0f;
	Left.Source.ViewportHeightPixels = 600;
	Left.OuterRadiusCells = 96;
	Left.VerticalHalfExtentCells = 32;
	FVoxelTerrainStageSource Right = Left;
	Right.Source.Center.X = 48;
	const TArray<FVoxelTerrainStageSource> Sources{Left, Right};
	const TArray<FVoxelTerrainStageSource> Reordered{Right, Left};
	FVoxelTerrainStagePlan First;
	FVoxelTerrainStagePlan Second;
	FString Error;
	if (!TestTrue(TEXT("Actual stage planner resolves both sources"),
		FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::VoxelProxy, Sources, 3, 64, -32, 64, 8.0, 17, 23, First, Error)) ||
		!TestTrue(TEXT("Source order can change without changing sampling policy"),
		FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::VoxelProxy, Reordered, 3, 64, -32, 64, 8.0, 17, 23, Second, Error))) return false;
	TestTrue(TEXT("Local SSE refines near geometry"), First.Pages.Num() > 2);
	TestEqual(TEXT("Source order preserves the complete geometry count"), First.Pages.Num(), Second.Pages.Num());
	for (const auto& Pair : First.Pages)
	{
		const FVoxelTerrainGeometry* Other = Second.Pages.Find(Pair.Key);
		TestTrue(TEXT("Source order preserves each actual sampling identity"), Other && *Other == Pair.Value);
		const FVoxelTerrainSpatialKey Reflected{Pair.Key.Representation,
			FIntVector(-Pair.Key.Coordinate.X - 1, Pair.Key.Coordinate.Y, Pair.Key.Coordinate.Z), Pair.Key.Level};
		const FVoxelTerrainGeometry* Mirror = First.Pages.Find(Reflected);
		TestTrue(TEXT("Mirrored sources preserve spatial and sampling symmetry"), Mirror && Mirror->SampleStepCells == Pair.Value.SampleStepCells);
		TestTrue(TEXT("Actual Proxy geometry obeys its representation floor"), Pair.Value.SampleStepCells >= 2 && Pair.Value.Validate(Error));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainNeighborBalanceTest, "WHFramework.Voxel.Rendering.Plan.NeighborBalance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainNeighborBalanceTest::RunTest(const FString& InParameters)
{
	FVoxelTerrainStageSource Source;
	Source.Source.Center = FIntVector(-8, 8, 8);
	Source.Source.VerticalFovDegrees = 90.0f;
	Source.Source.ViewportHeightPixels = 1080;
	Source.OuterRadiusCells = 256;
	Source.VerticalHalfExtentCells = 64;
	FVoxelTerrainStagePlan Plan;
	FString Error;
	if (!TestTrue(TEXT("Production coarse planner balances actual spatial pages"),
		FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::VoxelProxy, MakeArrayView(&Source, 1),
			4, 64, -64, 128, 8.0, 17, 23, Plan, Error))) return false;
	TestFalse(TEXT("Balanced demand contains geometry"), Plan.Pages.IsEmpty());
	for (const auto& Pair : Plan.Pages)
	{
		TestTrue(TEXT("Balanced page retains its own valid sampling identity"), Pair.Value.Validate(Error));
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			for (const int32 Sign : {-1, 1})
			{
				FVoxelTerrainSpatialKey Neighbor = Pair.Key;
				Neighbor.Coordinate[Axis] += Sign;
				while (Neighbor.Level <= 4 && !Plan.Pages.Contains(Neighbor))
				{
					Neighbor.Coordinate = FIntVector(int32(VoxelStreamingGeometry::FloorDivide(Neighbor.Coordinate.X, 2)),
						int32(VoxelStreamingGeometry::FloorDivide(Neighbor.Coordinate.Y, 2)),
						int32(VoxelStreamingGeometry::FloorDivide(Neighbor.Coordinate.Z, 2)));
					++Neighbor.Level;
				}
				TestTrue(TEXT("Every present face neighbour differs by at most one spatial level"),
					Neighbor.Level > 4 || Neighbor.Level <= Pair.Key.Level + 1);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainStageCapacityResumeTest,
	"WHFramework.Voxel.Rendering.Plan.StageCapacityResume", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainStageCapacityResumeTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(61, FGuid::NewGuid(), 1, {}, 1);
	auto External = Consumer;
	External.RequestSerial = 2;
	FVoxelTerrainResourceLimits Limits;
	Limits.WorkingBytes = 64 * 1024;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!TestTrue(TEXT("原 Consumer 创建真实账本"), FVoxelTerrainResources::Create(Limits, Consumer, Resources, Error))) return false;
	FVoxelTerrainStageCache Cache;
	Cache.Resources = Resources;
	Cache.Consumer = Consumer;
	FVoxelTerrainStageSource Source;
	Source.Source.VerticalFovDegrees = 90.0f;
	Source.Source.ViewportHeightPixels = 600;
	Source.OuterRadiusCells = 1;
	Source.VerticalHalfExtentCells = 1;
	const TSet<FVoxelTerrainSpatialKey> EmptyRoots;
	const uint64 RootBytes = VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelTerrainSpatialKey>(EmptyRoots, 4);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!TestTrue(TEXT("外部占用只给根枚举和少量剩余容量"), Resources->Acquire(External, EVoxelTerrainResourcePool::Working,
		Limits.WorkingBytes - RootBytes - 64, Blocker))) return false;
	FVoxelTerrainStagePlan Plan;
	const FVoxelTerrainSpatialKey OldKey{EVoxelTerrainRepresentation::Surface, FIntVector(7, 7, 0), 1};
	Plan.Pages.Add(OldKey, {OldKey, 1, -32, 64, 17, 23});
	TestFalse(TEXT("初次根失败不能当成空成功输出"), FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::Surface,
		MakeArrayView(&Source, 1), 1, 64, -32, 64, 8.0, 17, 23, Plan, Error, nullptr, {}, &Cache));
	TestTrue(TEXT("根准备暂时不足明确等待"), Error.StartsWith(TEXT("CapacityBlocked")));
	TestTrue(TEXT("失败保留原输出且不提交不完整缓存"), Plan.Pages.Num() == 1 && Plan.Pages.Contains(OldKey) && !Cache.Root);
	TestFalse(TEXT("自身临时额度退款不唤醒同一个 Consumer"), Cache.IsRetryReady());
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> OwnRefund;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, 64, OwnRefund)) return false;
	OwnRefund.Reset();
	TestFalse(TEXT("额外自身退款也不重试"), Cache.IsRetryReady());
	Blocker.Reset();
	TestTrue(TEXT("外部容量释放唤醒原根"), Cache.IsRetryReady());
	if (!TestTrue(TEXT("同一 Consumer 重试生成完整根"), FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::Surface,
		MakeArrayView(&Source, 1), 1, 64, -32, 64, 8.0, 17, 23, Plan, Error, nullptr, {}, &Cache))) return false;
	const uint64 NativeBytes = VoxelGenerationAllocation::AllocatedBytes(Plan.Pages) + VoxelGenerationAllocation::AllocatedBytes(Plan.Priorities) +
		VoxelGenerationAllocation::AllocatedBytes(Plan.Costs) + VoxelGenerationAllocation::AllocatedBytes(Plan.ResolutionFloorPages);
	TestTrue(TEXT("输出额度覆盖实际原生容器容量"), Plan.ResourceLease && Plan.ResourceLease->Bytes() >= NativeBytes && !Plan.Pages.IsEmpty());
	TestFalse(TEXT("成功后清除原容量等待"), Cache.IsRetryReady());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainStageUnachievableCapacityTest,
	"WHFramework.Voxel.Rendering.Plan.StageUnachievableCapacity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainStageUnachievableCapacityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(62, FGuid::NewGuid(), 1, {}, 1);
	FVoxelTerrainResourceLimits Limits;
	Limits.WorkingBytes = 1;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!FVoxelTerrainResources::Create(Limits, Consumer, Resources, Error)) return false;
	FVoxelTerrainStageCache Cache;
	Cache.Resources = Resources;
	Cache.Consumer = Consumer;
	FVoxelTerrainStageSource Source;
	Source.Source.VerticalFovDegrees = 90.0f;
	Source.Source.ViewportHeightPixels = 600;
	Source.OuterRadiusCells = 1;
	Source.VerticalHalfExtentCells = 1;
	FVoxelTerrainStagePlan Plan;
	TestFalse(TEXT("根容器超过硬上限时不分配"), FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::Surface,
		MakeArrayView(&Source, 1), 1, 64, -32, 64, 8.0, 17, 23, Plan, Error, nullptr, {}, &Cache));
	TestTrue(TEXT("硬上限失败明确不可达"), Error.StartsWith(TEXT("CapacityUnachievable")));
	TestFalse(TEXT("不可达根不注册外部容量重试"), Cache.bCapacityBlocked || Cache.IsRetryReady());
	TestEqual(TEXT("失败没有 Working 原生存储"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainDomainCompositionRetentionTest,
	"WHFramework.Voxel.Rendering.Plan.DomainCompositionRetention", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainDomainCompositionRetentionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(63, FGuid::NewGuid(), 1, {}, 1);
	auto External = Consumer;
	External.RequestSerial = 2;
	FVoxelTerrainResourceLimits Limits;
	Limits.WorkingBytes = 64 * 1024;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!FVoxelTerrainResources::Create(Limits, Consumer, Resources, Error)) return false;
	FVoxelTerrainDomainPlanner Planner;
	Planner.BindResources(Resources.ToSharedRef(), Consumer);
	FVoxelInterestSet Interest;
	auto Fine = MakeShared<FVoxelFinePagePlan, ESPMode::ThreadSafe>();
	Fine->Pages.Add({FIntVector::ZeroValue, 0});
	Interest.FinePagePlan = Fine;
	const FVoxelTerrainSpatialKey OldKey{EVoxelTerrainRepresentation::VoxelProxy, FIntVector(7), 1};
	const uint64 Bytes = VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FVoxelTerrainSpatialKey, FVoxelTerrainGeometry>>(Interest.Geometry, 1) +
		VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelTerrainSpatialKey>(Interest.ResolutionFloorPages, 1) +
		VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelViewKey>(Interest.VoxelProxy, 1) +
		VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelSurfaceTileKey>(Interest.Surface, 1) +
		VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelMacroTileKey>(Interest.Macro, 1);
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, Interest.CoarseCompositionLease)) return false;
	Interest.Geometry.Reserve(1); Interest.ResolutionFloorPages.Reserve(1); Interest.VoxelProxy.Reserve(1);
	Interest.Surface.Reserve(1); Interest.Macro.Reserve(1);
	Interest.Geometry.Add(OldKey, {OldKey, 2, -32, 64, 17, 23});
	Interest.ResolutionFloorPages.Add(OldKey);
	Interest.VoxelProxy.Add({FIntVector(7), 1});
	Interest.Surface.Add({FIntPoint(7, 7), 1});
	Interest.Macro.Add({FIntPoint(7, 7), 1});
	Interest.CoarseFailedRootCount = 7;
	const auto* OldLease = Interest.CoarseCompositionLease.Get();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!Resources->Acquire(External, EVoxelTerrainResourcePool::Working, Limits.WorkingBytes, Blocker)) return false;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -32; Manifest.Settings.MaxZ = 64;
	Manifest.BaseSampleHash = 17; Manifest.RegistryHash = 23;
	FVoxelViewSettings Settings;
	const TArray<FVoxelStreamingSource> Sources;
	TestFalse(TEXT("组合 scratch 不足时保留所有已接受输出"), FVoxelTerrainDomainPlanner::Build(Sources, Manifest, Settings, Interest, Error, &Planner));
	TestTrue(TEXT("失败保留原 Geometry、Floor、三表示与租约"), Interest.Geometry.Contains(OldKey) && Interest.ResolutionFloorPages.Contains(OldKey) &&
		Interest.VoxelProxy.Num() == 1 && Interest.Surface.Num() == 1 && Interest.Macro.Num() == 1 && Interest.CoarseCompositionLease.Get() == OldLease);
	TestEqual(TEXT("失败不覆盖原根诊断"), Interest.CoarseFailedRootCount, 7);
	TestFalse(TEXT("没有外部 release 时不重新创建组合"), Planner.HasRetryReady());
	Blocker.Reset();
	TestTrue(TEXT("外部 release 恢复原组合 Consumer"), Planner.HasRetryReady());
	if (!TestTrue(TEXT("容量恢复后原组合事务一次提交"), FVoxelTerrainDomainPlanner::Build(Sources, Manifest, Settings, Interest, Error, &Planner))) return false;
	TestTrue(TEXT("最终组合独立持有 Input 授额"), Interest.CoarseCompositionLease && Interest.CoarseCompositionLease->GetPool() == EVoxelTerrainResourcePool::Input);
	TestTrue(TEXT("成功提交 Fine Geometry 并退休原 coarse 集合"), Interest.Geometry.Num() == 1 && Interest.VoxelProxy.IsEmpty() &&
		Interest.Surface.IsEmpty() && Interest.Macro.IsEmpty() && Interest.ResolutionFloorPages.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainCoverageGrantOwnerTest,
	"WHFramework.Voxel.Rendering.Plan.CoverageGrantOwner", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainCoverageGrantOwnerTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(64, FGuid::NewGuid(), 1, {}, 99);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!FVoxelTerrainResources::Create({}, Consumer, Resources, Error)) return false;
	FVoxelCoverageIndex Index;
	Index.BindResources(Resources.ToSharedRef(), Consumer);
	FVoxelCoverageRecord Record;
	Record.Geometry = {{EVoxelTerrainRepresentation::Fine, {}, 0}, 1, -32, 64, 17, 23};
	if (!Record.Geometry.Key.TryBounds(-32, 64, Record.Min, Record.Max) || !Index.Upsert(Record, Error)) return false;
	FVoxelCoverageLocalSnapshot Snapshot;
	Index.Capture(Record.Min, Record.Max, Snapshot, false);
	if (!TestTrue(TEXT("真实 snapshot 捕获授额"), !Snapshot.bCountOnly && Snapshot.ResourceLease &&
		Snapshot.ResourceAdmission == EVoxelTerrainResourceAdmission::Granted)) return false;
	const uint64 BeforeRelease = Resources->ReleaseVersionExcludingOwner(Consumer);
	const uint64 Bytes = Snapshot.ResourceLease->Bytes();
	if (!Snapshot.ResourceLease->Resize(Bytes + 64) || !Snapshot.ResourceLease->Resize(Bytes)) return false;
	TestEqual(TEXT("snapshot 余量退款仍属于原 Consumer"), Resources->ReleaseVersionExcludingOwner(Consumer), BeforeRelease);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainCoverageUnachievableSnapshotTest,
	"WHFramework.Voxel.Rendering.Plan.CoverageUnachievableSnapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainCoverageUnachievableSnapshotTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelCoverageIndex Index;
	FVoxelCoverageRecord Record;
	Record.Geometry = {{EVoxelTerrainRepresentation::Fine, {}, 0}, 1, -32, 64, 17, 23};
	FString Error;
	if (!Record.Geometry.Key.TryBounds(-32, 64, Record.Min, Record.Max) || !Index.Upsert(Record, Error)) return false;
	FVoxelCoverageLocalSnapshot Counts;
	Index.CountCapture(Record.Min, Record.Max, Counts);
	const uint64 SnapshotBytes = FVoxelCoverageLocalSnapshot::ArrayMetadataBytes() +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe>, FDefaultAllocator>(Counts.MatchedRecords) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelCoverageRecordReadGuard, FDefaultAllocator>(Counts.MatchedRecords) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelCoverageReadGuard, FDefaultAllocator>(Counts.GuardCount);
	const auto Consumer = FVoxelGenerationConsumerKey::Section(65, FGuid::NewGuid(), 1, {}, 1);
	FVoxelTerrainResourceLimits Limits;
	Limits.InputBytes = VoxelBoundedAllocation::SharedBytes<FVoxelTerrainResources>() + SnapshotBytes - 1;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	if (!FVoxelTerrainResources::Create(Limits, Consumer, Resources, Error)) return false;
	Index.BindResources(Resources.ToSharedRef(), Consumer);
	FVoxelCoverageLocalSnapshot Snapshot;
	auto Capture = Index.CaptureSteps(Record.Min, Record.Max, Snapshot, false);
	for (int32 Step = 0; Step < 8192 && !Capture.IsComplete(); ++Step) Capture.Resume();
	TestTrue(TEXT("原生 snapshot 硬容量不可达立即结束 continuation"), Capture.IsComplete());
	TestTrue(TEXT("不可达不提供可提交读守卫"), Snapshot.bCountOnly && Snapshot.ResourceAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable);
	TestTrue(TEXT("失败保持原索引输入"), Index.Find(Record.Geometry.Key).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainDomainBoundedRootsTest,
	"WHFramework.Voxel.Rendering.Plan.DomainBoundedRoots", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainDomainBoundedRootsTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(66, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!FVoxelTerrainResources::Create({}, Consumer, Resources, Error)) return false;
	FVoxelTerrainDomainPlanner Domain;
	Domain.BindResources(Resources.ToSharedRef(), Consumer);
	FVoxelTerrainStageSource Source;
	Source.Source.Id = FGuid::NewGuid(); Source.Source.VerticalFovDegrees = 90.0f; Source.Source.ViewportHeightPixels = 600;
	Source.OuterRadiusCells = 160; Source.VerticalHalfExtentCells = 32;
	if (!Domain.ApplySourceDelta(EVoxelTerrainRepresentation::Surface, MakeArrayView(&Source, 1), Error)) return false;
	FVoxelTerrainStagePlan Plan;
	const FVoxelTerrainSpatialKey Previous{EVoxelTerrainRepresentation::Surface, FIntVector(100, 100, 0), 1};
	Plan.Pages.Add(Previous, {Previous, 1, -32, 64, 17, 23});
	TestFalse(TEXT("超过64根首次推进返回pending"), Domain.AdvanceDirtyRoots(EVoxelTerrainRepresentation::Surface,
		1, 64, -32, 64, 8.0, 17, 23, Plan, Error, nullptr, {}));
	TestTrue(*FString::Printf(TEXT("未完成明确报告 PlanningPending: %s"), *Error), Error.StartsWith(TEXT("PlanningPending")));
	TestTrue(TEXT("未完成保留原游标"), Domain.HasPendingWork());
	TestTrue(TEXT("原游标可继续推进"), Domain.HasRetryReady());
	TestTrue(TEXT("未完成保留旧输出"), Plan.Pages.Contains(Previous));
	bool Complete = false;
	for (int32 Step = 0; Step < 16 && !Complete; ++Step)
	{
		if (!Domain.ApplySourceDelta(EVoxelTerrainRepresentation::Surface, MakeArrayView(&Source, 1), Error)) return false;
		Complete = Domain.AdvanceDirtyRoots(EVoxelTerrainRepresentation::Surface, 1, 64, -32, 64, 8.0, 17, 23, Plan, Error, nullptr, {});
		if (!Complete && !Error.StartsWith(TEXT("PlanningPending"))) return false;
	}
	TestTrue(TEXT("重复相同source delta不重置游标且最终整体发布"), Complete && !Domain.HasPendingWork() && !Plan.Pages.Contains(Previous) && !Plan.Pages.IsEmpty());
	const int32 AcceptedPages = Plan.Pages.Num();
	if (!Domain.ApplySourceDelta(EVoxelTerrainRepresentation::Surface, MakeArrayView(&Source, 1), Error)) return false;
	TestTrue(TEXT("无变化输入直接使用持久根输出"), Domain.AdvanceDirtyRoots(EVoxelTerrainRepresentation::Surface,
		1, 64, -32, 64, 8.0, 17, 23, Plan, Error, nullptr, {}));
	TestEqual(TEXT("无变化页身份与数量保持"), Plan.Pages.Num(), AcceptedPages);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainCoverageRetainedUnionTest,
	"WHFramework.Voxel.Rendering.Plan.CoverageRetainedUnion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainCoverageRetainedUnionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(67, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!FVoxelTerrainResources::Create({}, Consumer, Resources, Error)) return false;
	FVoxelCoverageIndex Index;
	Index.BindResources(Resources.ToSharedRef(), Consumer);
	FVoxelCoverageRecord Record;
	Record.Geometry = {{EVoxelTerrainRepresentation::Fine, FIntVector(-1, 0, 0), 0}, 1, -32, 64, 17, 23};
	if (!Record.Geometry.Key.TryBounds(-32, 64, Record.Min, Record.Max) || !Index.Upsert(Record, Error)) return false;
	Record.Geometry.Key.Coordinate.X = 1;
	if (!Record.Geometry.Key.TryBounds(-32, 64, Record.Min, Record.Max) || !Index.Upsert(Record, Error)) return false;
	const FVoxelCoverageIndex Frozen = Index;
	uint64 OldBytes = 0, AliasBytes = 0; int32 OldCount = 0, AliasCount = 0;
	if (!Frozen.AccumulateRetainedInput(*Resources, nullptr, {}, OldBytes, OldCount) ||
		!Index.AccumulateRetainedInput(*Resources, &Frozen, {}, AliasBytes, AliasCount)) return false;
	TestEqual(TEXT("同一个冻结root引用不重复算Input"), AliasBytes, OldBytes);
	TestEqual(TEXT("同一个冻结root引用不重复算handle"), AliasCount, OldCount);
	++Record.Stamp.ContentRevision;
	if (!Index.Upsert(Record, Error)) return false;
	uint64 NewBytes = 0, UnionBytes = 0; int32 NewCount = 0, UnionCount = 0;
	if (!Index.AccumulateRetainedInput(*Resources, nullptr, {}, NewBytes, NewCount) ||
		!Index.AccumulateRetainedInput(*Resources, &Frozen, {}, UnionBytes, UnionCount)) return false;
	TestTrue(TEXT("变化路径旧新保活，共享半空间子树只算一次"), UnionBytes > FMath::Max(OldBytes, NewBytes) && UnionBytes < OldBytes + NewBytes &&
		UnionCount > FMath::Max(OldCount, NewCount) && UnionCount < OldCount + NewCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainCoverageUniqueRetirementTest,
	"WHFramework.Voxel.Rendering.Plan.CoverageUniqueRetirement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainCoverageUniqueRetirementTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(68, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!FVoxelTerrainResources::Create({}, Consumer, Resources, Error)) return false;
	FVoxelCoverageIndex Index;
	Index.BindResources(Resources.ToSharedRef(), Consumer);
	FVoxelCoverageRecord Record;
	Record.Geometry = {{EVoxelTerrainRepresentation::Fine, {}, 0}, 1, -32, 64, 17, 23};
	if (!Record.Geometry.Key.TryBounds(-32, 64, Record.Min, Record.Max) || !Index.Upsert(Record, Error)) return false;
	FVoxelCoverageIndex Frozen = Index;
	TestFalse(TEXT("冻结读者存在不得原地改写"), Index.CanRemoveWithoutAllocation(Record.Geometry.Key));
	Frozen.Reset();
	TestTrue(TEXT("最后冻结读者离开后无事件也可查询免费退休机会"), Index.CanRemoveWithoutAllocation(Record.Geometry.Key));
	Index.MarkPublishedInputs();
	TestTrue(TEXT("原authority在零新授额路径退休"), Index.Remove(Record.Geometry.Key, Error));
	TestEqual(TEXT("独占路径退休后目录为空"), Index.NumRecords(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainStageRootFailureIsolationTest,
	"WHFramework.Voxel.Rendering.Plan.StageRootFailureIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainStageRootFailureIsolationTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(69, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!FVoxelTerrainResources::Create({}, Consumer, Resources, Error)) return false;
	FVoxelTerrainStageCache Cache;
	Cache.Resources = Resources;
	Cache.Consumer = Consumer;
	FVoxelTerrainStageSource Sources[2];
	for (auto& Source : Sources)
	{
		Source.Source.Id = FGuid::NewGuid();
		Source.Source.VerticalFovDegrees = 90.0f;
		Source.Source.ViewportHeightPixels = 1;
		Source.OuterRadiusCells = 1;
		Source.VerticalHalfExtentCells = 1;
	}
	Sources[1].Source.Center.X = 4096;
	FVoxelTerrainPageCostLimits CostLimits;
	CostLimits.Triangles = 512;
	FVoxelTerrainStagePlan Plan;
	const auto Build = [&]()
	{
		return FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::Surface, MakeArrayView(Sources),
			1, 64, -32, 64, 8.0, 17, 23, Plan, Error, nullptr, CostLimits, &Cache);
	};
	if (!Build()) return false;
	const FVoxelTerrainSpatialKey FailedKey{EVoxelTerrainRepresentation::Surface, {}, 1};
	const FVoxelTerrainSpatialKey OldValidKey{EVoxelTerrainRepresentation::Surface, FIntVector(128, 0, 0), 1};
	const FVoxelTerrainSpatialKey NewValidKey{EVoxelTerrainRepresentation::Surface, FIntVector(130, 0, 0), 1};
	if (!Plan.Pages.Contains(FailedKey) || !Plan.Pages.Contains(OldValidKey)) return false;
	const FVoxelTerrainGeometry RetainedGeometry = Plan.Pages.FindChecked(FailedKey);
	Sources[0].Source.ViewportHeightPixels = 600;
	Sources[1].Source.Center.X = 4160;
	TestTrue(TEXT("一根最小页成本失败仍可完成其他根"), Build());
	TestTrue(TEXT("失败根保留原已接受Geometry"), Plan.Pages.Contains(FailedKey) && Plan.Pages.FindChecked(FailedKey) == RetainedGeometry);
	TestTrue(TEXT("有效根独立移动并退休其旧根"), Plan.Pages.Contains(NewValidKey) && !Plan.Pages.Contains(OldValidKey));
	TestTrue(TEXT("失败诊断可见且永久失败不等待外部退租"), Plan.FailedRootCount > 0 && !Cache.HasPendingWork() && !Cache.IsRetryReady());
	const uint64 Version = Cache.GetOutputVersion();
	const int32 Rebuilt = Cache.RebuiltNodes;
	TestTrue(TEXT("相同失败输入仍可读取原混合输出"), Build());
	TestEqual(TEXT("永久失败不产生新attempt输出版本"), Cache.GetOutputVersion(), Version);
	TestEqual(TEXT("永久失败不重复重建根"), Cache.RebuiltNodes, Rebuilt);
	FVoxelTerrainStageCache EmptyCache;
	EmptyCache.Resources = Resources;
	EmptyCache.Consumer = Consumer;
	FVoxelTerrainStagePlan EmptyPlan;
	TestFalse(TEXT("初次失败没有旧根不得以空输出成功"), FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::Surface,
		MakeArrayView(Sources, 1), 1, 64, -32, 64, 8.0, 17, 23, EmptyPlan, Error, nullptr, CostLimits, &EmptyCache));
	TestTrue(TEXT("初次失败保留未发布状态"), EmptyPlan.Pages.IsEmpty() && EmptyCache.GetOutputVersion() == 0);
	return true;
}

#endif
