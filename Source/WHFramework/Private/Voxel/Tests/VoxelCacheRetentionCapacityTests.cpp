#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"

namespace
{
	void AdvanceCacheRetentionMaintenance(FVoxelGenerationPlanCache& InCache)
	{
		for (int32 Index = 0; Index < FVoxelGenerationPlanCache::ShardCount * 2; ++Index)
		{
			InCache.TickMaintenance(256);
		}
	}

	bool FillCacheRetentionInput(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutBlocker)
	{
		const FVoxelTerrainResourceSnapshot State = InResources->Snapshot();
		const uint64 Used = State.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)];
		const uint64 HandleBytes = VoxelBoundedAllocation::SharedBytes<FVoxelTerrainResourceLease>();
		if (Used >= State.Limits.InputBytes || State.Limits.InputBytes - Used <= HandleBytes)
		{
			return false;
		}
		return InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input,
			State.Limits.InputBytes - Used - HandleBytes, OutBlocker);
	}

	bool SeedCacheRetentionArea(FVoxelGenerationPlanCache& InCache, const int32 InOrigin, FString& OutError)
	{
		const FIntPoint Position(InOrigin, InOrigin);
		FVoxelBaseColumnEntryPtr Base;
		FVoxelNaturalColumnEntryPtr Natural;
		if (!InCache.GetOrBuildBaseColumn(Position,
			[Position](FVoxelBaseColumnEntry& OutEntry, FString&)
			{
				OutEntry.Position = Position;
				return true;
			}, Base, OutError) ||
			!InCache.GetOrBuildNaturalColumn(Position,
			[Position](FVoxelNaturalColumnEntry& OutEntry, FString&)
			{
				OutEntry.Position = Position;
				return true;
			}, Natural, OutError))
		{
			return false;
		}

		FVoxelHydrologyRegionKey HydrologyKey;
		HydrologyKey.Coordinate = Position / 16;
		InCache.StoreHydrology(HydrologyKey, MakeShared<FVoxelHydrologyPlan, ESPMode::ThreadSafe>());
		FVoxelGenerationTileKey PlanKey;
		PlanKey.Coordinate = FIntVector(InOrigin / 256, InOrigin / 256, 0);
		InCache.StoreCave(PlanKey, MakeShared<FVoxelCavePlan, ESPMode::ThreadSafe>());
		InCache.StoreFeature(PlanKey, MakeShared<FVoxelFeaturePlan, ESPMode::ThreadSafe>());
		InCache.StoreStructure(PlanKey, MakeShared<FVoxelStructurePlan, ESPMode::ThreadSafe>());
		FVoxelEcologyTileKey EcologyKey;
		EcologyKey.Coordinate = Position / 64;
		FVoxelEcologyPlanPtr Ecology;
		return InCache.GetOrBuildEcology(EcologyKey,
			[](FVoxelEcologyPlan&, FString&)
			{
				return true;
			}, Ecology, OutError);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCacheRetentionCapacityAdmissionTest,
	"WHFramework.Voxel.Generation.CacheRetention.CapacityAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCacheRetentionCapacityAdmissionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(37, FGuid::NewGuid(), 1, {}, 1);
	FVoxelTerrainResourceLimits Limits;
	Limits.InputBytes = 1;
	if (!TestTrue(TEXT("配置真实有限Input池"), Resources->Configure(Limits)))
	{
		return false;
	}
	FVoxelGenerationCacheRetentionPoint Point;
	Point.Center = FIntPoint(4, 4);
	Point.HydrologyRadiusCells = 1;
	FVoxelGenerationCacheRetention Retention;
	Retention.Revision = 1;
	Retention.HydrologyRegionSide = 8;
	Cache->UpdateRetention(Retention);
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Lease;
	FString Error;
	const auto Before = Resources->Snapshot();
	TestFalse(TEXT("无Input授予不能创建保留节点"), Cache->RetainForTask(Point, Resources, Consumer, Lease, Error));
	TestTrue(TEXT("固定Input上限小于必需对象和句柄时返回永久不可达原因"), Error.StartsWith(TEXT("CapacityUnachievable:")));
	TestFalse(TEXT("拒收不产生租约"), Lease.IsValid());
	const auto Rejected = Resources->Snapshot();
	TestEqual(TEXT("拒收没有先分配账本"), Rejected.LedgerMetadataBytes, uint64(0));
	TestEqual(TEXT("拒收没有授额"), Rejected.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)], uint64(0));
	TestEqual(TEXT("拒收不伪造释放事件"), Rejected.ReleaseVersion, Before.ReleaseVersion);
	if (!Resources->Configure({}) || !TestTrue(TEXT("取得容量后创建节点"),
		Cache->RetainForTask(Point, Resources, Consumer, Lease, Error)))
	{
		return false;
	}
	const auto Admitted = Resources->Snapshot();
	const uint64 RetentionBytes = FVoxelGenerationCacheRetentionLease::GetRequiredAllocationBytes();
	TestEqual(TEXT("节点和带授予deleter的控制器按真实分配模型计入原Input池"),
		Admitted.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)],
		Admitted.LedgerMetadataBytes + Admitted.LeaseHandleBytes + RetentionBytes);
	const auto OriginalLease = Lease;
	FVoxelHydrologyRegionKey OriginalKey;
	FVoxelHydrologyRegionKey UnadmittedKey;
	UnadmittedKey.Coordinate = FIntPoint(128, 128);
	Cache->StoreHydrology(OriginalKey, MakeShared<FVoxelHydrologyPlan, ESPMode::ThreadSafe>());
	Cache->StoreHydrology(UnadmittedKey, MakeShared<FVoxelHydrologyPlan, ESPMode::ThreadSafe>());
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!TestTrue(TEXT("填满同一Input池"), FillCacheRetentionInput(Resources, Consumer, Blocker)))
	{
		return false;
	}
	const auto Full = Resources->Snapshot();
	Point.Center = FIntPoint(1028, 1028);
	TestFalse(TEXT("容量不足时拒绝替换节点"), Cache->RetainForTask(Point, Resources, Consumer, Lease, Error));
	TestTrue(TEXT("固定上限可达但暂时占满时返回可恢复原因"), Error.StartsWith(TEXT("CapacityBlocked:")));
	TestTrue(TEXT("拒收保留原OutLease"), Lease == OriginalLease);
	TestEqual(TEXT("拒收没有分配另一资源句柄"), Resources->Snapshot().LeaseCount, Full.LeaseCount);
	TestEqual(TEXT("拒收不改变原池容量"),
		Resources->Snapshot().Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)],
		Full.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)]);
	TestFalse(TEXT("不完整Consumer不能取得节点"), Cache->RetainForTask(Point, Resources, {}, Lease, Error));
	TestTrue(TEXT("身份拒收保留原OutLease及明确原因"),
		Lease == OriginalLease && Error.StartsWith(TEXT("InvalidConsumer:")));
	AdvanceCacheRetentionMaintenance(*Cache);
	FVoxelHydrologyPlanPtr Plan;
	TestTrue(TEXT("拒收仍保留原节点的空间"), Cache->FindHydrology(OriginalKey, Plan));
	TestFalse(TEXT("拒收不登记未接纳的空间"), Cache->FindHydrology(UnadmittedKey, Plan));
	Blocker.Reset();
	TestTrue(TEXT("同一池释放后可重新接纳"), Cache->RetainForTask(Point, Resources, Consumer, Lease, Error));
	TestTrue(TEXT("成功才替换OutLease并清空原因"), Lease != OriginalLease && Error.IsEmpty());
	TestEqual(TEXT("旧引用仍存活时两个节点各自收费"), Resources->Snapshot().LeaseCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCacheRetentionIndependentLifetimeTest,
	"WHFramework.Voxel.Generation.CacheRetention.IndependentLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCacheRetentionIndependentLifetimeTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	FVoxelGenerationCacheRetention Retention;
	Retention.Revision = 1;
	Retention.HydrologyRegionSide = 8;
	Cache->UpdateRetention(Retention);
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Head;
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Middle;
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Tail;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(37, FGuid::NewGuid(), 1, {}, 1);
	auto OtherOwner = Consumer;
	OtherOwner.OwnerId = FGuid::NewGuid();
	auto OtherWorld = Consumer;
	++OtherWorld.WorldEpoch;
	FVoxelGenerationCacheRetentionPoint Point;
	Point.HydrologyRadiusCells = 1;
	FString Error;
	Point.Center = FIntPoint(4, 4);
	if (!Cache->RetainForTask(Point, Resources, Consumer, Head, Error))
	{
		return false;
	}
	Point.Center = FIntPoint(44, 4);
	if (!Cache->RetainForTask(Point, Resources, OtherOwner, Middle, Error))
	{
		return false;
	}
	Point.Center = FIntPoint(84, 4);
	if (!Cache->RetainForTask(Point, Resources, OtherWorld, Tail, Error))
	{
		return false;
	}
	const auto Charged = Resources->Snapshot();
	Cache->Reset();
	TestEqual(TEXT("Reset不提前释放仍活着的任务节点容量"),
		Resources->Snapshot().Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)],
		Charged.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)]);
	FVoxelHydrologyRegionKey HeadKey;
	FVoxelHydrologyRegionKey MiddleKey;
	MiddleKey.Coordinate = FIntPoint(5, 0);
	FVoxelHydrologyRegionKey TailKey;
	TailKey.Coordinate = FIntPoint(10, 0);
	Cache->StoreHydrology(HeadKey, MakeShared<FVoxelHydrologyPlan, ESPMode::ThreadSafe>());
	Cache->StoreHydrology(MiddleKey, MakeShared<FVoxelHydrologyPlan, ESPMode::ThreadSafe>());
	Cache->StoreHydrology(TailKey, MakeShared<FVoxelHydrologyPlan, ESPMode::ThreadSafe>());
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> MiddleReader = Middle;
	Middle.Reset();
	AdvanceCacheRetentionMaintenance(*Cache);
	FVoxelHydrologyPlanPtr Plan;
	TestTrue(TEXT("中间节点的最后读者仍保留事实"), Cache->FindHydrology(MiddleKey, Plan));
	TestEqual(TEXT("普通引用释放不提前退容量"), Resources->Snapshot().LeaseCount, 3);
	const uint64 ExternalBefore = Resources->ReleaseVersionExcludingOwner(Consumer);
	MiddleReader.Reset();
	const auto MiddleReleased = Resources->Snapshot();
	TestEqual(TEXT("中间节点只退自己的对象和资源句柄"),
		Charged.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)] -
		MiddleReleased.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)],
		FVoxelGenerationCacheRetentionLease::GetRequiredAllocationBytes() +
		VoxelBoundedAllocation::SharedBytes<FVoxelTerrainResourceLease>());
	TestTrue(TEXT("不同Owner的真实释放可唤醒原Owner"),
		Resources->ReleaseVersionExcludingOwner(Consumer) > ExternalBefore);
	AdvanceCacheRetentionMaintenance(*Cache);
	TestFalse(TEXT("中间节点独立撤销"), Cache->FindHydrology(MiddleKey, Plan));
	TestTrue(TEXT("中间解链保留前节点"), Cache->FindHydrology(HeadKey, Plan));
	TestTrue(TEXT("中间解链保留后节点"), Cache->FindHydrology(TailKey, Plan));
	Head.Reset();
	AdvanceCacheRetentionMaintenance(*Cache);
	TestFalse(TEXT("头节点独立撤销"), Cache->FindHydrology(HeadKey, Plan));
	TestTrue(TEXT("尾节点不受不同Owner和World节点撤销影响"), Cache->FindHydrology(TailKey, Plan));
	const uint64 OtherWorldBefore = Resources->ReleaseVersionExcludingOwner(Consumer);
	Tail.Reset();
	TestTrue(TEXT("同Owner的不同World释放仍属于外部事件"),
		Resources->ReleaseVersionExcludingOwner(Consumer) > OtherWorldBefore);
	AdvanceCacheRetentionMaintenance(*Cache);
	TestFalse(TEXT("尾节点独立撤销"), Cache->FindHydrology(TailKey, Plan));
	TestEqual(TEXT("全部节点析构后句柄归零"), Resources->Snapshot().LeaseCount, 0);
	TestEqual(TEXT("空原账本仍准确保留自身容量"),
		Resources->Snapshot().Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)],
		Resources->Snapshot().LedgerMetadataBytes);
	Point.Center = FIntPoint(4, 4);
	TestTrue(TEXT("链表清空后可重新接纳头尾节点"), Cache->RetainForTask(Point, Resources, Consumer, Head, Error));
	Cache->StoreHydrology(HeadKey, MakeShared<FVoxelHydrologyPlan, ESPMode::ThreadSafe>());
	AdvanceCacheRetentionMaintenance(*Cache);
	TestTrue(TEXT("重新接纳节点仍参与维护"), Cache->FindHydrology(HeadKey, Plan));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCacheRetentionMaintenanceSemanticsTest,
	"WHFramework.Voxel.Generation.CacheRetention.MaintenanceSemantics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCacheRetentionMaintenanceSemanticsTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	FString Error;
	if (!SeedCacheRetentionArea(*Cache, 0, Error) || !SeedCacheRetentionArea(*Cache, 1024, Error))
	{
		AddError(Error);
		return false;
	}
	AdvanceCacheRetentionMaintenance(*Cache);
	TestEqual(TEXT("Revision为零不启动淘汰"), Cache->GetStats().Hydrology, 2);
	FVoxelGenerationCacheRetention Streaming;
	Streaming.Revision = 1;
	Streaming.HydrologyRegionSide = 8;
	Streaming.HydrologyCellSize = 2;
	FVoxelGenerationCacheRetentionPoint& SourcePoint = Streaming.Points.AddDefaulted_GetRef();
	SourcePoint.NaturalRadiusCells = 32;
	SourcePoint.PlanRadiusCells = 32;
	SourcePoint.HydrologyRadiusCells = 8;
	Cache->UpdateRetention(Streaming);
	FVoxelGenerationCacheRetentionPoint TaskPoint = SourcePoint;
	TaskPoint.Center = FIntPoint(1024, 1024);
	const auto Consumer = FVoxelGenerationConsumerKey::Section(37, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Lease;
	if (!Cache->RetainForTask(TaskPoint, Resources, Consumer, Lease, Error))
	{
		AddError(Error);
		return false;
	}
	AdvanceCacheRetentionMaintenance(*Cache);
	const auto Retained = Cache->GetStats();
	TestEqual(TEXT("Source与任务共同保留Base"), Retained.BaseColumns, 2);
	TestEqual(TEXT("Source与任务共同保留Natural"), Retained.NaturalColumns, 2);
	TestEqual(TEXT("Hydrology沿用RegionSide乘CellSize及闭区间半径"), Retained.Hydrology, 2);
	TestEqual(TEXT("Cave沿用Tile原点与Plan半径"), Retained.Caves, 2);
	TestEqual(TEXT("Ecology沿用64格中心与Plan半径"), Retained.Ecology, 2);
	TestEqual(TEXT("Feature沿用Plan保留"), Retained.Features, 2);
	TestEqual(TEXT("Structure沿用Plan保留"), Retained.Structures, 2);
	Lease.Reset();
	AdvanceCacheRetentionMaintenance(*Cache);
	const auto SourceRetained = Cache->GetStats();
	TestEqual(TEXT("任务退出后Source仍保留Base"), SourceRetained.BaseColumns, 1);
	TestEqual(TEXT("任务退出后Source仍保留Natural"), SourceRetained.NaturalColumns, 1);
	TestEqual(TEXT("任务退出后Source仍保留Hydrology"), SourceRetained.Hydrology, 1);
	TestEqual(TEXT("任务退出后Source仍保留Cave"), SourceRetained.Caves, 1);
	TestEqual(TEXT("任务退出后Source仍保留Ecology"), SourceRetained.Ecology, 1);
	TestEqual(TEXT("任务退出后Source仍保留Feature"), SourceRetained.Features, 1);
	TestEqual(TEXT("任务退出后Source仍保留Structure"), SourceRetained.Structures, 1);
	Streaming.Points.Reset();
	++Streaming.Revision;
	Cache->UpdateRetention(Streaming);
	AdvanceCacheRetentionMaintenance(*Cache);
	const auto Released = Cache->GetStats();
	TestEqual(TEXT("Source退出后七类事实均可增量淘汰"), Released.BaseColumns + Released.NaturalColumns +
		Released.Hydrology + Released.Caves + Released.Ecology + Released.Features + Released.Structures, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCacheRetentionControllerLifetimeTest,
	"WHFramework.Voxel.Generation.CacheRetention.ControllerLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCacheRetentionControllerLifetimeTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(37, FGuid::NewGuid(), 1, {}, 1);
	FVoxelGenerationCacheRetention Retention;
	Retention.Revision = 1;
	Retention.HydrologyRegionSide = 8;
	Cache->UpdateRetention(Retention);
	FVoxelGenerationCacheRetentionPoint Point;
	Point.Center = FIntPoint(4, 4);
	Point.HydrologyRadiusCells = 1;
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Lease;
	FString Error;
	if (!TestTrue(TEXT("控制器生命周期由原资源授予覆盖"),
		Cache->RetainForTask(Point, Resources, Consumer, Lease, Error)))
	{
		return false;
	}
	const uint64 ActualObjectBytes = FMemory::GetAllocSize(Lease.Get());
	TestTrue(TEXT("对象真实allocator容量不超过分配前的量化授予"), ActualObjectBytes > 0 &&
		ActualObjectBytes <= FMemory::QuantizeSize(sizeof(FVoxelGenerationCacheRetentionLease), alignof(FVoxelGenerationCacheRetentionLease)));
	const auto Charged = Resources->Snapshot();
	TWeakPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> WeakLease = Lease;
	FVoxelHydrologyRegionKey Key;
	Cache->StoreHydrology(Key, MakeShared<FVoxelHydrologyPlan, ESPMode::ThreadSafe>());
	Lease.Reset();
	TestFalse(TEXT("最后强引用释放后对象已退出"), WeakLease.IsValid());
	const auto WeakRetained = Resources->Snapshot();
	TestEqual(TEXT("weak仍持有控制器时原资源句柄保留"), WeakRetained.LeaseCount, 1);
	TestEqual(TEXT("对象析构不能提前归还控制器及其原授予"),
		WeakRetained.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)],
		Charged.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)]);
	TestEqual(TEXT("对象解链不伪造容量释放事件"), WeakRetained.ReleaseVersion, Charged.ReleaseVersion);
	AdvanceCacheRetentionMaintenance(*Cache);
	FVoxelHydrologyPlanPtr Plan;
	TestFalse(TEXT("仅weak引用不能继续空间保留"), Cache->FindHydrology(Key, Plan));
	WeakLease.Reset();
	const auto Released = Resources->Snapshot();
	TestEqual(TEXT("最后weak退出才归还原资源句柄"), Released.LeaseCount, 0);
	TestEqual(TEXT("对象与控制器退出后仅原账本backing仍收费"),
		Released.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Released.LedgerMetadataBytes);
	TestTrue(TEXT("控制器真实退出后才产生释放事件"), Released.ReleaseVersion > Charged.ReleaseVersion);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCacheRetentionResourceOwnerIsolationTest,
	"WHFramework.Voxel.Generation.CacheRetention.ResourceOwnerIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCacheRetentionResourceOwnerIsolationTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache =
		MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	const auto OtherCache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto OtherResources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(37, FGuid::NewGuid(), 1, {}, 1);
	const auto OtherConsumer = FVoxelGenerationConsumerKey::Section(38, FGuid::NewGuid(), 1, {}, 1);
	FVoxelGenerationCacheRetentionPoint Point;
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Lease;
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> OtherLease;
	FString Error;
	if (!Cache->RetainForTask(Point, Resources, Consumer, Lease, Error) ||
		!OtherCache->RetainForTask(Point, OtherResources, OtherConsumer, OtherLease, Error))
	{
		return false;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!FillCacheRetentionInput(Resources, Consumer, Blocker))
	{
		return false;
	}
	const auto OtherBefore = OtherResources->Snapshot();
	TestFalse(TEXT("原World容量不足不能借用其它World池"),
		Cache->RetainForTask(Point, Resources, Consumer, Lease, Error));
	TestEqual(TEXT("拒收不触碰其它World账本"), OtherResources->Snapshot().Version, OtherBefore.Version);
	TWeakPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> CacheObserver = Cache;
	Cache.Reset();
	TestTrue(TEXT("节点强引用使原Cache保持到任务真正结束"), CacheObserver.IsValid());
	Lease.Reset();
	TestFalse(TEXT("最后任务结束后原Cache可析构"), CacheObserver.IsValid());
	TestEqual(TEXT("一个World退出不释放其它World节点"), OtherResources->Snapshot().LeaseCount, 1);
	TestTrue(TEXT("其它World节点仍有效"), OtherLease.IsValid());
	Blocker.Reset();
	OtherLease.Reset();
	TestEqual(TEXT("各World容量由自己的实际持有者结算"), OtherResources->Snapshot().LeaseCount, 0);
	return true;
}

#endif
