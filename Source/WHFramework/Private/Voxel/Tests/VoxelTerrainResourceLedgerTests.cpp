#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

namespace
{
	template<int32 InBytes, uint32 InAlignment = 1>
	struct alignas(InAlignment) FVoxelSharedAllocationProbe
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
		const uint64 SetModel = FMath::Max<uint64>(VoxelGenerationAllocation::AllocatedBytes(Values), VoxelGenerationAllocation::Private::SetReserveBytes<uint64>(Values, Count));
		const uint64 MapModel = FMath::Max<uint64>(VoxelGenerationAllocation::AllocatedBytes(Identities), VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FGuid, uint64>>(Identities, Count));
		const uint64 SparseModel = FMath::Max<uint64>(VoxelGenerationAllocation::AllocatedBytes(Slots), VoxelGenerationAllocation::Private::SparseArrayReserveBytes<FGuid>(Slots, Count));
		Values.Reserve(Count);
		Identities.Reserve(Count);
		Slots.Reserve(Count);
		TestTrue(TEXT("Set 的槽位、BitArray、Hash 真实容量不超过分配前模型"), uint64(VoxelGenerationAllocation::AllocatedBytes(Values)) <= SetModel);
		TestTrue(TEXT("Map 的真实容量不超过分配前模型"), uint64(VoxelGenerationAllocation::AllocatedBytes(Identities)) <= MapModel);
		TestTrue(TEXT("SparseArray 的真实容量不超过分配前模型"), uint64(VoxelGenerationAllocation::AllocatedBytes(Slots)) <= SparseModel);
		for (int32 Index = Values.Num(); Index < Count; ++Index) Values.Add(uint64(Index));
		TestTrue(TEXT("预留后填充不绕过 Set 容量授予"), uint64(VoxelGenerationAllocation::AllocatedBytes(Values)) <= SetModel);
	}
	// 独立复现已经有 heap 位图的增长，不能仅验证从空容器首次 Reserve。
	TSet<uint64> GrowingValues;
	TMap<FGuid, uint64> GrowingIdentities;
	TSparseArray<FGuid> GrowingSlots;
	for (const int32 Count : {256, 512, 1024})
	{
		const uint64 SetModel = VoxelGenerationAllocation::Private::SetReserveBytes<uint64>(GrowingValues, Count);
		const uint64 MapModel = VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FGuid, uint64>>(GrowingIdentities, Count);
		const uint64 SparseModel = VoxelGenerationAllocation::Private::SparseArrayReserveBytes<FGuid>(GrowingSlots, Count);
		GrowingValues.Reserve(Count);
		GrowingIdentities.Reserve(Count);
		GrowingSlots.Reserve(Count);
		TestTrue(TEXT("已有 heap 位图的 Set 扩容不超过原授予模型"), uint64(VoxelGenerationAllocation::AllocatedBytes(GrowingValues)) <= SetModel);
		TestTrue(TEXT("已有 heap 位图的 Map 扩容不超过原授予模型"), uint64(VoxelGenerationAllocation::AllocatedBytes(GrowingIdentities)) <= MapModel);
		TestTrue(TEXT("已有 heap 位图的 SparseArray 扩容不超过原授予模型"), uint64(VoxelGenerationAllocation::AllocatedBytes(GrowingSlots)) <= SparseModel);
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
		TestTrue(*FString::Printf(TEXT("SharedBytes 在分配前覆盖控制器的真实 allocator 容量 size=%llu align=%llu actual=%llu granted=%llu default=%llu"), uint64(sizeof(*Controller)), uint64(alignof(decltype(*Controller))), ActualBytes, GrantedBytes, uint64(FMemory::QuantizeSize(sizeof(*Controller)))), ActualBytes <= GrantedBytes);
		Controller->ReleaseSharedReference();
	};
	Check(FVoxelSharedAllocationProbe<1>());
	Check(FVoxelSharedAllocationProbe<17>());
	Check(FVoxelSharedAllocationProbe<65>());
	Check(FVoxelSharedAllocationProbe<513>());
	Check(FVoxelSharedAllocationProbe<17, 64>());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelResourceReservedPublicationTest,
	"WHFramework.Voxel.FineRefactor.Resources.ReservedPublication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelResourceReservedPublicationTest::RunTest(const FString& InParameters)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	FVoxelTerrainResourceLimits Limits;
	Limits.WorkingBytes = 4096;
	Limits.ResidentBytes = 8192;
	Limits.OverlapBytes = 4096;
	if (!Resources->Configure(Limits)) return false;
	const auto Attempt = FVoxelGenerationConsumerKey::Section(37, FGuid::NewGuid(), 1, {}, 19);
	auto CacheOwner = Attempt;
	CacheOwner.Kind = EVoxelGenerationConsumerKind::WorldFact;
	CacheOwner.OwnerId = FGuid::NewGuid();
	CacheOwner.RequestSerial = 1;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Body;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Standby;
	if (!Resources->Acquire(Attempt, EVoxelTerrainResourcePool::Working, 4096, Body, 2) ||
		!Resources->Acquire(Attempt, EVoxelTerrainResourcePool::Resident, 4096, Standby, 1)) return false;
	const auto Before = Resources->Snapshot();
	const uint64 Serial = Body->Serial();
	const uint64 AttemptExternal = Resources->ReleaseVersionExcludingOwner(Attempt);
	TestTrue(TEXT("Resident 已满时消费原 standby，不请求第二份额度"),
		Body->CommitReservation(Standby.ToSharedRef(), CacheOwner));
	const auto Published = Resources->Snapshot();
	TestEqual(TEXT("正文仍是原租约 serial"), Body->Serial(), Serial);
	TestEqual(TEXT("正文保留完整字节"), Body->Bytes(), uint64(4096));
	TestEqual(TEXT("只消费目标池 standby"), Standby->Bytes(), uint64(0));
	TestEqual(TEXT("Working 已归还"), Published.Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	TestEqual(TEXT("Resident 没有重复授额"), Published.Used[int32(EVoxelTerrainResourcePool::Resident)], uint64(4096));
	TestEqual(TEXT("提交不创建句柄或新 backing"), Published.Used[int32(EVoxelTerrainResourcePool::Input)],
		Before.Used[int32(EVoxelTerrainResourcePool::Input)]);
	TestEqual(TEXT("两个租约的预授予拆分继续有效"), Published.ReservedSplitCount, Before.ReservedSplitCount);
	TestEqual(TEXT("Working 转账不伪造原任务的外部唤醒"),
		Resources->ReleaseVersionExcludingOwner(Attempt), AttemptExternal);
	Standby.Reset();
	TestEqual(TEXT("失败尝试原 standby 句柄退款也不自唤醒"),
		Resources->ReleaseVersionExcludingOwner(Attempt), AttemptExternal);
	auto Child = Body->Split(256);
	if (!TestTrue(TEXT("正文提交后仍能消费原拆分保证"), Child.IsValid())) return false;
	Body.Reset();
	TestEqual(TEXT("缓存正文退休只留下真实子正文"),
		Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Resident)], uint64(256));
	TestTrue(TEXT("真实缓存释放以新稳定所有者唤醒原任务"),
		Resources->ReleaseVersionExcludingOwner(Attempt) > AttemptExternal);
	const uint64 CacheExternal = Resources->ReleaseVersionExcludingOwner(CacheOwner);
	Child.Reset();
	const auto Retired = Resources->Snapshot();
	TestEqual(TEXT("子租约继承发布后的所有者，不自唤醒缓存"),
		Resources->ReleaseVersionExcludingOwner(CacheOwner), CacheExternal);
	TestEqual(TEXT("正文完整退休"), Retired.Used[int32(EVoxelTerrainResourcePool::Resident)], uint64(0));
	TestEqual(TEXT("拆分保证完整归还"), Retired.ReservedSplitCount, 0);
	TestEqual(TEXT("句柄完整退休"), Retired.LeaseCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelResourceReservedPublicationRefusalTest,
	"WHFramework.Voxel.FineRefactor.Resources.ReservedPublicationRefusal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelResourceReservedPublicationRefusalTest::RunTest(const FString& InParameters)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto Foreign = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	if (!Resources->Configure({}) || !Foreign->Configure({})) return false;
	const auto Attempt = FVoxelGenerationConsumerKey::Section(37, FGuid::NewGuid(), 1, {}, 19);
	auto CacheOwner = Attempt;
	CacheOwner.OwnerId = FGuid::NewGuid();
	auto OtherAttempt = Attempt;
	++OtherAttempt.RequestSerial;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Body;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Standby;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SmallStandby;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> OtherStandby;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ForeignStandby;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SamePoolStandby;
	if (!Resources->Acquire(Attempt, EVoxelTerrainResourcePool::Working, 4096, Body) ||
		!Resources->Acquire(Attempt, EVoxelTerrainResourcePool::Resident, 4096, Standby) ||
		!Resources->Acquire(Attempt, EVoxelTerrainResourcePool::Resident, 2048, SmallStandby) ||
		!Resources->Acquire(OtherAttempt, EVoxelTerrainResourcePool::Resident, 4096, OtherStandby) ||
		!Resources->Acquire(Attempt, EVoxelTerrainResourcePool::Working, 4096, SamePoolStandby) ||
		!Foreign->Acquire(Attempt, EVoxelTerrainResourcePool::Resident, 4096, ForeignStandby)) return false;
	const auto Before = Resources->Snapshot();
	const auto ForeignBefore = Foreign->Snapshot();
	TestFalse(TEXT("拒绝正文自己充当 standby"), Body->CommitReservation(Body.ToSharedRef(), CacheOwner));
	TestFalse(TEXT("拒绝消费其它账本的额度"), Body->CommitReservation(ForeignStandby.ToSharedRef(), CacheOwner));
	TestFalse(TEXT("拒绝不足的 standby"), Body->CommitReservation(SmallStandby.ToSharedRef(), CacheOwner));
	TestFalse(TEXT("同 owner 的另一 request 也不能借用额度"), Body->CommitReservation(OtherStandby.ToSharedRef(), CacheOwner));
	TestFalse(TEXT("拒绝在同池伪造发布转账"), Body->CommitReservation(SamePoolStandby.ToSharedRef(), CacheOwner));
	auto OtherWorld = CacheOwner;
	++OtherWorld.WorldEpoch;
	TestFalse(TEXT("拒绝跨 World 发布"), Body->CommitReservation(Standby.ToSharedRef(), OtherWorld));
	TestFalse(TEXT("拒绝无效的新 owner"), Body->CommitReservation(Standby.ToSharedRef(), {}));
	const auto After = Resources->Snapshot();
	TestEqual(TEXT("拒收保留原正文"), Body->Bytes(), uint64(4096));
	TestEqual(TEXT("拒收保留原 standby"), Standby->Bytes(), uint64(4096));
	TestEqual(TEXT("拒收没有改版本"), After.Version, Before.Version);
	TestEqual(TEXT("拒收没有伪造释放"), After.ReleaseVersion, Before.ReleaseVersion);
	TestEqual(TEXT("拒收保留 Working"), After.Used[int32(EVoxelTerrainResourcePool::Working)],
		Before.Used[int32(EVoxelTerrainResourcePool::Working)]);
	TestEqual(TEXT("拒收保留 Resident"), After.Used[int32(EVoxelTerrainResourcePool::Resident)],
		Before.Used[int32(EVoxelTerrainResourcePool::Resident)]);
	TestEqual(TEXT("其它账本同样无副作用"), Foreign->Snapshot().Version, ForeignBefore.Version);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelResourceRequiredCapacityClassificationTest,
	"WHFramework.Voxel.FineRefactor.Resources.RequiredCapacityClassification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelResourceRequiredCapacityClassificationTest::RunTest(const FString& InParameters)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	FVoxelTerrainResourceLimits Limits;
	Limits.InputBytes = 1;
	if (!Resources->Configure(Limits)) return false;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(51, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const auto Before = Resources->Snapshot();
	TestTrue(TEXT("Payload 小但 native ledger/全部拆分句柄永远无法容纳"),
		Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Working, 1, Lease, 7) == EVoxelTerrainResourceAdmission::CapacityUnachievable);
	TestFalse(TEXT("永久拒收不构造租约"), Lease.IsValid());
	TestEqual(TEXT("永久拒收不先创建账本"), Resources->Snapshot().LedgerMetadataBytes, uint64(0));
	TestEqual(TEXT("永久拒收不伪造释放事件"), Resources->Snapshot().ReleaseVersion, Before.ReleaseVersion);

	Limits.InputBytes = 1024 * 1024;
	Limits.WorkingBytes = 4096;
	if (!Resources->Configure(Limits)) return false;
	auto Other = Consumer;
	++Other.RequestSerial;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (Resources->TryAcquire(Other, EVoxelTerrainResourcePool::Working, Limits.WorkingBytes, Blocker) != EVoxelTerrainResourceAdmission::Granted) return false;
	TestTrue(TEXT("外部可释放占用仅报告暂时阻塞"),
		Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Working, 1024, Lease) == EVoxelTerrainResourceAdmission::CapacityBlocked);
	const uint64 WaitingVersion = Resources->ReleaseVersionExcludingOwner(Consumer);
	Blocker.Reset();
	TestTrue(TEXT("完整其它 Consumer 的释放可唤醒原尝试"), Resources->ReleaseVersionExcludingOwner(Consumer) > WaitingVersion);
	if (Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Working, 1024, Lease) != EVoxelTerrainResourceAdmission::Granted) return false;
	const uint64 RequiredVersion = Resources->Snapshot().Version;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Candidate;
	TestTrue(TEXT("声明的不可撤销 Input 下限包含新 native 句柄，不能等外部退款"),
		Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Working, 1, Candidate, 0, Limits.InputBytes, 1) == EVoxelTerrainResourceAdmission::CapacityUnachievable);
	TestEqual(TEXT("必需峰值拒收不改账本"), Resources->Snapshot().Version, RequiredVersion);
	TestTrue(TEXT("原租约增长超固定 Working 上限明确不可达"),
		Lease->TryResize(Limits.WorkingBytes + 1) == EVoxelTerrainResourceAdmission::CapacityUnachievable);
	TestEqual(TEXT("拒收增长保留原 payload"), Lease->Bytes(), uint64(1024));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelResourceFrozenWorldDomainTest,
	"WHFramework.Voxel.FineRefactor.Resources.FrozenWorldDomain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelResourceFrozenWorldDomainTest::RunTest(const FString& InParameters)
{
	const auto Control = FVoxelGenerationConsumerKey::Section(73, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!TestTrue(TEXT("生产工厂冻结原世界"), FVoxelTerrainResources::Create({}, Control, Resources, Error))) return false;
	const auto Before = Resources->Snapshot();
	auto Foreign = Control;
	++Foreign.WorldEpoch;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TestTrue(TEXT("另一世界的合法消费者不能借原账本"), Resources->TryAcquire(Foreign,
		EVoxelTerrainResourcePool::Working, 256, Lease) == EVoxelTerrainResourceAdmission::InvalidRequest);
	TestFalse(TEXT("跨世界拒收不构造句柄"), Lease.IsValid());
	TestEqual(TEXT("跨世界拒收不扩容控制表"), Resources->Snapshot().LedgerMetadataBytes, Before.LedgerMetadataBytes);
	TestEqual(TEXT("跨世界拒收不改变原版本"), Resources->Snapshot().Version, Before.Version);
	auto SameWorld = Control;
	SameWorld.OwnerId = FGuid::NewGuid();
	if (!TestTrue(TEXT("同世界的独立原所有者可正常授额"), Resources->TryAcquire(SameWorld,
		EVoxelTerrainResourcePool::Working, 256, Lease) == EVoxelTerrainResourceAdmission::Granted)) return false;
	Lease.Reset();
	TestEqual(TEXT("原消费者退出后无遗留句柄"), Resources->Snapshot().LeaseCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNativeAllocatorCapacityTest,
	"WHFramework.Voxel.FineRefactor.Resources.NativeAllocatorCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNativeAllocatorCapacityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	for (const uint64 Size : {uint64(1), uint64(24), uint64(40), uint64(88), uint64(536),
		uint64(1023), uint64(65535), uint64(131072), uint64(131073), uint64(524287),
		uint64(524288), uint64(2097152), uint64(8388608), uint64(33554433)})
	{
		for (const uint32 Alignment : {uint32(DEFAULT_ALIGNMENT), uint32(8), uint32(16), uint32(64)})
		{
			const uint64 Granted = VoxelBoundedAllocation::QuantizeBytes(Size, Alignment);
			void* Allocation = FMemory::Malloc(Size, Alignment);
			const uint64 Actual = FMemory::GetAllocSize(Allocation);
			TestTrue(*FString::Printf(TEXT("分配前上界覆盖真实 heap 块 size=%llu align=%u actual=%llu granted=%llu"),
				Size, Alignment, Actual, Granted), Actual >= Size && Actual <= Granted);
			const uint64 GrownSize = Size + 17;
			const uint64 GrowthGranted = VoxelBoundedAllocation::QuantizeBytes(GrownSize, Alignment);
			Allocation = FMemory::Realloc(Allocation, GrownSize, Alignment);
			const uint64 GrowthActual = FMemory::GetAllocSize(Allocation);
			TestTrue(*FString::Printf(TEXT("扩容上界覆盖真实 Realloc 块 size=%llu align=%u actual=%llu granted=%llu"),
				GrownSize, Alignment, GrowthActual, GrowthGranted), GrowthActual >= GrownSize && GrowthActual <= GrowthGranted);
			FMemory::Free(Allocation);
		}
	}
	return true;
}

#endif
