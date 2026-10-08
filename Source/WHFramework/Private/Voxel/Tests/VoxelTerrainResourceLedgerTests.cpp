#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"

namespace
{
	template<int32 InBytes>
	struct FVoxelSharedAllocationProbe
	{
		uint8 Payload[InBytes] = {};
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelResourceLedgerPrepaidPathTest,
	"WHFramework.Voxel.FineRefactor.Resources.PrepaidPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelResourceLedgerPrepaidPathTest::RunTest(const FString& InParameters)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	Resources->Configure({});
	const auto Consumer = FVoxelGenerationConsumerKey::Section(37, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Path, Blocker;
	constexpr uint64 Payload = 1024 * 1024;
	if (!TestTrue(TEXT("路径、33个句柄及账本容量统一预授予"),
		Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident, Payload, Path, 33))) return false;
	auto State = Resources->Snapshot();
	TestEqual(TEXT("尚未生成子句柄"), State.LeaseCount, 1);
	TestEqual(TEXT("完整固定路径的拆分容量"), State.ReservedSplitCount, 33);
	TestEqual(TEXT("Input 包含账本及所有预授予句柄"), State.Used[int32(EVoxelTerrainResourcePool::Input)],
		State.LedgerMetadataBytes + State.LeaseHandleBytes);
	const uint64 Available = State.Limits.InputBytes - State.Used[int32(EVoxelTerrainResourcePool::Input)] -
		VoxelBoundedAllocation::SharedBytes<FVoxelTerrainResourceLease>();
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Available, Blocker)) return false;
	const uint64 FullInput = Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)];
	TArray<TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>, TInlineAllocator<33>> Children;
	for (int32 Index = 0; Index < 33; ++Index)
	{
		auto Child = Path->Split(64);
		if (!TestTrue(TEXT("Input 已满仍可消费先前预授予的路径拆分"), Child.IsValid())) return false;
		Children.Add(MoveTemp(Child));
	}
	State = Resources->Snapshot();
	TestEqual(TEXT("拆分没有再次授予 Input"), State.Used[int32(EVoxelTerrainResourcePool::Input)], FullInput);
	TestEqual(TEXT("拆分不重复授予 Payload"), State.Used[int32(EVoxelTerrainResourcePool::Resident)], Payload);
	TestEqual(TEXT("路径额度全部消费"), State.ReservedSplitCount, 0);
	Path.Reset();
	TestEqual(TEXT("父句柄退休只释放剩余 Payload"),
		Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Resident)], uint64(33 * 64));
	Children.Reset();
	Blocker.Reset();
	State = Resources->Snapshot();
	TestEqual(TEXT("所有 Payload 已释放"), State.Used[int32(EVoxelTerrainResourcePool::Resident)], uint64(0));
	TestEqual(TEXT("所有句柄已释放"), State.LeaseHandleBytes, uint64(0));
	TestEqual(TEXT("保留的空账本仍如实收费"), State.Used[int32(EVoxelTerrainResourcePool::Input)], State.LedgerMetadataBytes);
	TestTrue(TEXT("无租约时允许释放账本并重配置"), Resources->Configure({}));
	TestEqual(TEXT("旧账本容量真实释放"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)], uint64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelResourceLedgerCapacityIsolationTest,
	"WHFramework.Voxel.FineRefactor.Resources.MetadataCapacityIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelResourceLedgerCapacityIsolationTest::RunTest(const FString& InParameters)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	FVoxelTerrainResourceLimits Limits;
	Limits.InputBytes = 1;
	Resources->Configure(Limits);
	const auto Before = Resources->Snapshot();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TestFalse(TEXT("元数据不足不能偷偷创建 Resident 租约"), Resources->Acquire(
		FVoxelGenerationConsumerKey::Section(37, FGuid::NewGuid(), 1, {}, 1),
		EVoxelTerrainResourcePool::Resident, 4096, Lease, 33));
	const auto After = Resources->Snapshot();
	TestFalse(TEXT("拒收没有句柄"), Lease.IsValid());
	TestEqual(TEXT("拒收没有先分配账本"), After.LedgerMetadataBytes, uint64(0));
	TestEqual(TEXT("拒收没有授予 Payload"), After.Used[int32(EVoxelTerrainResourcePool::Resident)], uint64(0));
	TestEqual(TEXT("拒收不伪造容量释放事件"), After.ReleaseVersion, Before.ReleaseVersion);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelReservedContainerCapacityTest,
	"WHFramework.Voxel.FineRefactor.Resources.ReservedContainerCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelReservedContainerCapacityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	TSet<uint64> Values;
	TMap<FGuid, uint64> Identities;
	TSparseArray<FGuid> Slots;
	for (const int32 Count : {1, 5, 127, 128, 129, 255, 257, 511, 513, 4097})
	{
		const uint64 SetModel = FMath::Max<uint64>(Values.GetAllocatedSize(), VoxelBoundedAllocation::SetBytes<uint64>(Count));
		const uint64 MapModel = FMath::Max<uint64>(Identities.GetAllocatedSize(), VoxelBoundedAllocation::MapBytes<FGuid, uint64>(Count));
		const uint64 SparseModel = FMath::Max<uint64>(Slots.GetAllocatedSize(), VoxelBoundedAllocation::SparseArrayBytes<FGuid>(Count));
		Values.Reserve(Count);
		Identities.Reserve(Count);
		Slots.Reserve(Count);
		TestTrue(TEXT("Set 的槽位、BitArray、Hash 真实容量不超过分配前模型"), uint64(Values.GetAllocatedSize()) <= SetModel);
		TestTrue(TEXT("Map 的真实容量不超过分配前模型"), uint64(Identities.GetAllocatedSize()) <= MapModel);
		TestTrue(TEXT("SparseArray 的真实容量不超过分配前模型"), uint64(Slots.GetAllocatedSize()) <= SparseModel);
		for (int32 Index = Values.Num(); Index < Count; ++Index) Values.Add(uint64(Index));
		TestTrue(TEXT("预留后填充不绕过 Set 容量授予"), uint64(Values.GetAllocatedSize()) <= SetModel);
	}
	// 独立复现已经有 heap 位图的增长，不能仅验证从空容器首次 Reserve。
	TSet<uint64> GrowingValues;
	TMap<FGuid, uint64> GrowingIdentities;
	TSparseArray<FGuid> GrowingSlots;
	for (const int32 Count : {256, 512, 1024})
	{
		const uint64 SetModel = VoxelBoundedAllocation::SetBytes<uint64>(Count);
		const uint64 MapModel = VoxelBoundedAllocation::MapBytes<FGuid, uint64>(Count);
		const uint64 SparseModel = VoxelBoundedAllocation::SparseArrayBytes<FGuid>(Count);
		GrowingValues.Reserve(Count);
		GrowingIdentities.Reserve(Count);
		GrowingSlots.Reserve(Count);
		TestTrue(TEXT("已有 heap 位图的 Set 扩容不超过原授予模型"), uint64(GrowingValues.GetAllocatedSize()) <= SetModel);
		TestTrue(TEXT("已有 heap 位图的 Map 扩容不超过原授予模型"), uint64(GrowingIdentities.GetAllocatedSize()) <= MapModel);
		TestTrue(TEXT("已有 heap 位图的 SparseArray 扩容不超过原授予模型"), uint64(GrowingSlots.GetAllocatedSize()) <= SparseModel);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSharedAllocationCapacityTest,
	"WHFramework.Voxel.FineRefactor.Resources.SharedControllerCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSharedAllocationCapacityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Check = [this](auto InProbe)
	{
		using FProbe = decltype(InProbe);
		// 与 MakeShared 相同的真实 UE 控制器分配；检查 allocator 实际块容量。
		auto* Controller = SharedPointerInternals::NewIntrusiveReferenceController<ESPMode::ThreadSafe, FProbe>();
		const uint64 ActualBytes = FMemory::GetAllocSize(Controller);
		const uint64 GrantedBytes = VoxelBoundedAllocation::SharedBytes<FProbe>();
		TestTrue(TEXT("分配器报告了控制器实际容量"), ActualBytes > 0);
		TestTrue(TEXT("SharedBytes 在分配前覆盖控制器的真实 allocator 容量"), ActualBytes <= GrantedBytes);
		Controller->ReleaseSharedReference();
	};
	Check(FVoxelSharedAllocationProbe<1>());
	Check(FVoxelSharedAllocationProbe<17>());
	Check(FVoxelSharedAllocationProbe<65>());
	Check(FVoxelSharedAllocationProbe<513>());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelResourceExternalReleaseTest,
	"WHFramework.Voxel.FineRefactor.Resources.ExternalRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelResourceExternalReleaseTest::RunTest(const FString& InParameters)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	Resources->Configure({});
	const auto SaveOwner = FVoxelGenerationConsumerKey::Section(37, FGuid::NewGuid(), 1, {}, 1);
	auto OtherOwner = SaveOwner;
	OtherOwner.OwnerId = FGuid::NewGuid();
	const uint64 Before = Resources->ReleaseVersionExcludingOwner(SaveOwner);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SaveLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> OtherLease;
	if (!Resources->Acquire(SaveOwner, EVoxelTerrainResourcePool::Working, 4096, SaveLease) ||
		!Resources->Acquire(OtherOwner, EVoxelTerrainResourcePool::Working, 4096, OtherLease)) return false;
	SaveLease->Resize(2048);
	SaveLease.Reset();
	TestEqual(TEXT("本保存失败清理不触发自身重试"), Resources->ReleaseVersionExcludingOwner(SaveOwner), Before);
	OtherLease->Resize(2048);
	const uint64 Released = Resources->ReleaseVersionExcludingOwner(SaveOwner);
	TestTrue(TEXT("原任务运行期间的外部释放仍可观察"), Released > Before);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		if (!Resources->Acquire(SaveOwner, EVoxelTerrainResourcePool::Working, 128, SaveLease)) return false;
		SaveLease.Reset();
	}
	TestEqual(TEXT("后续自身清理不能覆盖已发生的外部释放"), Resources->ReleaseVersionExcludingOwner(SaveOwner), Released);
	const uint64 OtherExternal = Resources->ReleaseVersionExcludingOwner(OtherOwner);
	OtherLease.Reset();
	TestEqual(TEXT("其它 owner 同样排除自己的清理"), Resources->ReleaseVersionExcludingOwner(OtherOwner), OtherExternal);
	auto NextIncarnation = SaveOwner;
	++NextIncarnation.OwnerIncarnation;
	TestTrue(TEXT("新 incarnation 的释放属于原 owner 的外部事件"),
		Resources->Acquire(NextIncarnation, EVoxelTerrainResourcePool::Working, 128, SaveLease));
	SaveLease.Reset();
	TestTrue(TEXT("身份比较包含 incarnation"), Resources->ReleaseVersionExcludingOwner(SaveOwner) > Released);
	return true;
}

#endif
