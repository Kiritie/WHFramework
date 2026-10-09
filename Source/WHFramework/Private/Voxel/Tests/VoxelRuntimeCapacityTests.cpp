#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Rendering/VoxelTerrainStatistics.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Save/VoxelRegionStore.h"
#include "Voxel/Streaming/VoxelEmergeManager.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

namespace
{
	bool FillPool(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources,
		EVoxelTerrainResourcePool Pool, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutBlocker)
	{
		const auto State = Resources->Snapshot();
		const uint64 Limits[] = {State.Limits.InputBytes, State.Limits.WorkingBytes, State.Limits.StagedBytes,
			State.Limits.ResidentBytes - State.Limits.OverlapBytes, State.Limits.OverlapBytes};
		return Resources->Acquire(FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1), Pool,
			Limits[int32(Pool)] - State.Used[int32(Pool)], OutBlocker);
	}

	bool PublishEmpty(FVoxelWorldRuntime& Runtime, const FIntVector& Key, FString& Error)
	{
		auto* Section = Runtime.FindOrAllocate(Key, 1);
		if (!Section) return false;
		TArray<FVoxelBlockState> Base;
		Base.Init(FVoxelBlockState(), 4096);
		return Runtime.PublishBase(Key, Section->Stamp, MoveTemp(Base), Error) && Runtime.PublishFinal(Key, 0, {}, {}, Error);
	}

	class FWaitForDiscard final : public IAutomationLatentCommand
	{
	public:
		FWaitForDiscard(TSharedRef<FVoxelTerrainStatistics, ESPMode::ThreadSafe> InStatistics,
			const uint64 InDiscarded, FAutomationTestBase& InTest)
			: Statistics(InStatistics), Discarded(InDiscarded), Test(InTest), Deadline(FPlatformTime::Seconds() + 5.0)
		{
		}

		virtual bool Update() override
		{
			if (Statistics->GetWorkAccounting().Discarded >= Discarded) return true;
			if (FPlatformTime::Seconds() < Deadline) return false;
			Test.AddError(TEXT("退休的未接纳输出未在真实后台析构后结算Discarded"));
			return true;
		}

	private:
		TSharedRef<FVoxelTerrainStatistics, ESPMode::ThreadSafe> Statistics;
		uint64 Discarded;
		FAutomationTestBase& Test;
		double Deadline;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelRuntimeAllocationAdmissionTest,
	"WHFramework.Voxel.Runtime.AllocationAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelRuntimeAllocationAdmissionTest::RunTest(const FString&)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	FVoxelWorldRuntime Runtime(1, true, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	Runtime.BindTerrainResources(Resources);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!TestTrue(TEXT("填满普通驻留容量"), FillPool(Resources, EVoxelTerrainResourcePool::Resident, Blocker))) return false;
	TestNull(TEXT("没有授予时不创建Section"), Runtime.FindOrAllocate({}, 1));
	TestEqual(TEXT("失败不改驻留目录"), Runtime.NumSections(), 0);
	TestEqual(TEXT("失败不创建稀疏扫描槽"), Runtime.GetResidencyOrder().Num(), 0);
	Blocker.Reset();
	auto* Section = Runtime.FindOrAllocate({}, 1);
	if (!TestNotNull(TEXT("释放容量后创建Section"), Section)) return false;
	TestEqual(TEXT("失败不消费Section代号"), Section->Stamp.Token, 1ull);
	TestTrue(TEXT("完成槽与元数据均已收费"), Section->MetadataLease && Section->PendingPublish);
	const auto Stamp = Section->Stamp;
	TArray<FVoxelBlockState> Base;
	const uint64 BaseBytes = VoxelBoundedAllocation::WorkerSharedBytes<TArray<FVoxelBlockState>>() +
		VoxelBoundedAllocation::ArrayBytes<FVoxelBlockState>(4096);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BaseLease;
	if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), Stamp.Token, {}, 1),
		EVoxelTerrainResourcePool::Staged, BaseBytes, BaseLease)) return false;
	Base.Reserve(4096);
	Base.SetNum(4096);
	if (!FillPool(Resources, EVoxelTerrainResourcePool::Resident, Blocker)) return false;
	FString Error;
	TestFalse(TEXT("自然基底接纳等待容量"), Runtime.PublishBase({}, Stamp, MoveTemp(Base), Error, BaseLease));
	TestTrue(TEXT("容量失败是可恢复等待"), Error.StartsWith(TEXT("CapacityBlocked:")));
	TestEqual(TEXT("失败保留生成输出"), Base.Num(), 4096);
	TestTrue(TEXT("失败保留Allocated且无已发布基底"), Section->Status == EVoxelSectionStatus::Allocated && !Section->BaseBlocks);
	Blocker.Reset();
	if (!TestTrue(TEXT("同一输出恢复接纳"), Runtime.PublishBase({}, Stamp, MoveTemp(Base), Error, BaseLease))) return false;
	const auto Frozen = Section->BaseBlocks;
	TestTrue(TEXT("Runtime撤销可释放Section"), Runtime.RemoveSection({}));
	TestEqual(TEXT("冻结读者保留旧基底"), Frozen->Num(), 4096);
	TestTrue(TEXT("冻结基底仍持有原租约"), BaseLease->Bytes() >= BaseBytes);
	Section = Runtime.FindOrAllocate({}, 2);
	if (!Section) return false;
	TestTrue(TEXT("同一坐标新代不能接受旧输出"), Section->Stamp.Token > Stamp.Token && !Runtime.IsCurrentStamp({}, Stamp));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelRuntimePreparedCapacityTransactionTest,
	"WHFramework.Voxel.Runtime.PreparedCapacityTransaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelRuntimePreparedCapacityTransactionTest::RunTest(const FString&)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	FVoxelWorldRuntime Runtime(1, true, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	Runtime.BindTerrainResources(Resources);
	const FIntVector A(0, 0, 0), B(1, 0, 0);
	FString Error;
	if (!PublishEmpty(Runtime, A, Error) || !PublishEmpty(Runtime, B, Error)) return false;
	FVoxelSectionSnapshot Frozen;
	const uint64 SnapshotBytes = VoxelBoundedAllocation::ArrayBytes<uint32>(4096) +
		6 * VoxelBoundedAllocation::ArrayBytes<uint32>(256);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Input;
	if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, A, 1),
		EVoxelTerrainResourcePool::Input, SnapshotBytes, Input)) return false;
	const uint64 BeforeCapture = Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)];
	if (!Runtime.CaptureSnapshot(A, Frozen, Input)) return false;
	TestEqual(TEXT("复用捕获租约不重复收费"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)], BeforeCapture);
	TestTrue(TEXT("快照绑定传入租约"), Frozen.ResourceLease == Input);
	const TArray<FVoxelCellEdit> Cells{{A * 16, {}, FVoxelBlockState(1, 0)}, {B * 16, {}, FVoxelBlockState(1, 0)}};
	FVoxelPreparedEdit Prepared;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!FillPool(Resources, EVoxelTerrainResourcePool::Working, Blocker)) return false;
	TestFalse(TEXT("准备阶段在复制前等待工作区"), Runtime.PrepareEdit(Cells, {}, Prepared, Error));
	TestTrue(TEXT("准备失败保留两个Current"), Runtime.FindSection(A)->Blocks[0].IsAir() && Runtime.FindSection(B)->Blocks[0].IsAir());
	Blocker.Reset();
	if (!TestTrue(TEXT("完整批次准备"), Runtime.PrepareEdit(Cells, {}, Prepared, Error))) return false;
	const FGuid Transaction = Prepared.TransactionId;
	if (!FillPool(Resources, EVoxelTerrainResourcePool::Working, Blocker)) return false;
	FVoxelEditBatch Batch;
	TestFalse(TEXT("提交缺少Journal事务工作区时整体等待"), Runtime.CommitPreparedEdit(MoveTemp(Prepared), Batch, Error));
	TestEqual(TEXT("等待不丢失已准备批次"), Prepared.Sections.Num(), 2);
	TestEqual(TEXT("等待保持事务身份"), Prepared.TransactionId, Transaction);
	TestEqual(TEXT("等待不推进Cut"), Runtime.GetAuthorityCommitCut(), 0ull);
	TestEqual(TEXT("等待不发布第一个Section"), Runtime.FindSection(A)->CommittedRevision, 0ull);
	TestEqual(TEXT("等待不发布第二个Section"), Runtime.FindSection(B)->CommittedRevision, 0ull);
	Blocker.Reset();
	if (!TestTrue(TEXT("同一候选原子恢复提交"), Runtime.CommitPreparedEdit(MoveTemp(Prepared), Batch, Error))) return false;
	TestEqual(TEXT("两个Section共享一次Cut"), Runtime.GetAuthorityCommitCut(), 1ull);
	TestEqual(TEXT("同Region只推进一次序号"), Runtime.GetOverlayRegionSequence({}), 1ull);
	TestEqual(TEXT("Delta保留事务身份"), Batch.TransactionId, Transaction);
	TestEqual(TEXT("第一个Section提交"), Runtime.FindSection(A)->CommittedRevision, 1ull);
	TestEqual(TEXT("第二个Section提交"), Runtime.FindSection(B)->CommittedRevision, 1ull);
	TestEqual(TEXT("冻结捕获不被编辑修改"), Frozen.Blocks[0], 0u);
	TestTrue(TEXT("Delta持有实际输出容量"), Batch.ResourceLease && Batch.ResourceLease->Bytes() >= Batch.Sections.GetAllocatedSize());
	if (!Runtime.PrepareEdit({}, {}, Prepared, Error)) return false;
	TestTrue(TEXT("空编辑可提交"), Runtime.CommitPreparedEdit(MoveTemp(Prepared), Batch, Error));
	TestEqual(TEXT("空编辑不推进Cut"), Runtime.GetAuthorityCommitCut(), 1ull);
	TestTrue(TEXT("空编辑不生成Delta"), Batch.Sections.IsEmpty());
	FVoxelWorldRuntime Replica(1, false, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	if (!PublishEmpty(Replica, A, Error) || !PublishEmpty(Replica, B, Error)) return false;
	FVoxelSectionSnapshot RemoteSnapshot;
	TestFalse(TEXT("没有Replica当前版本证明不能捕获远端正文"), Replica.CaptureSnapshot(A, RemoteSnapshot));
	Replica.SetRemoteSectionValidator([A](const FIntVector& Section, const uint64 Revision)
	{
		return Section == A && Revision == 0;
	});
	if (!TestTrue(TEXT("当前Replica证明允许中心捕获"), Replica.CaptureSnapshot(A, RemoteSnapshot))) return false;
	TestFalse(TEXT("DataReady但Replica过期的邻居必须未知"), RemoteSnapshot.Known[0]);
	Replica.SetRemoteSectionValidator([](const FIntVector&, uint64) { return false; });
	const auto RemoteLease = RemoteSnapshot.ResourceLease;
	TestFalse(TEXT("Replica切换后不能把旧正文标记为新快照"), Replica.CaptureSnapshot(A, RemoteSnapshot));
	TestTrue(TEXT("拒绝捕获不覆盖原冻结输入"), RemoteSnapshot.ResourceLease == RemoteLease && RemoteSnapshot.Blocks.Num() == 4096);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelDeltaDecodeCapacityContinuationTest,
	"WHFramework.Voxel.Save.DeltaDecodeCapacityContinuation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelDeltaDecodeCapacityContinuationTest::RunTest(const FString&)
{
	const auto Registry = VoxelTest::MakeRegistry();
	FVoxelWorldManifest Manifest;
	Manifest.WorldId = FGuid::NewGuid();
	Manifest.RecipeHash = 17;
	FVoxelPersistentSection Original;
	Original.Section = FIntVector(-9, 7, 3);
	Original.Revision = 19;
	Original.Blocks.Add(0, FVoxelBlockState(1, 0));
	TArray<uint8> Wire;
	if (!FVoxelDeltaCodec::Encode(Manifest, *Registry, Original, Wire)) return false;
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, Original.Section, 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!FillPool(Resources, EVoxelTerrainResourcePool::Staged, Blocker)) return false;
	FVoxelDeltaDecoder Decoder;
	TSharedPtr<FVoxelPersistentSection, ESPMode::ThreadSafe> Decoded;
	FString Error;
	TestFalse(TEXT("解压计数后等待输出容量"), Decoder.Advance(Wire, Manifest, *Registry, Resources, Consumer,
		EVoxelTerrainResourcePool::Staged, Decoded, Error));
	TestTrue(TEXT("容量失败保留Raw且未产生正文"), Error.StartsWith(TEXT("CapacityBlocked:")) &&
		Decoder.GetRetainedWorkingBytes() > 0 && !Decoded);
	const uint64 Working = Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)];
	TestFalse(TEXT("相同等待可续行"), Decoder.Advance(Wire, Manifest, *Registry, Resources, Consumer,
		EVoxelTerrainResourcePool::Staged, Decoded, Error));
	TestEqual(TEXT("等待不会重复解压分配"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], Working);
	Blocker.Reset();
	if (!TestTrue(TEXT("释放后按原切点解码"), Decoder.Advance(Wire, Manifest, *Registry, Resources, Consumer,
		EVoxelTerrainResourcePool::Staged, Decoded, Error))) return false;
	TestEqual(TEXT("输出Section完整"), Decoded->Section, Original.Section);
	TestEqual(TEXT("输出内容版本完整"), Decoded->Revision, Original.Revision);
	TestEqual(TEXT("冻结工作区在完成后释放"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], 0ull);
	TestTrue(TEXT("输出按真实小正文收费"), Decoded->ResourceLease && Decoded->ResourceLease->Bytes() < 4096);
	TestTrue(TEXT("输出正文正确"), Decoded->Blocks.OrderIndependentCompareEqual(Original.Blocks));
	FVoxelDeltaDecoder Invalid;
	FVoxelWorldManifest WrongWorld = Manifest;
	WrongWorld.WorldId = FGuid::NewGuid();
	const auto Previous = Decoded;
	TestFalse(TEXT("另一个世界不能替换已完成输出"), Invalid.Advance(Wire, WrongWorld, *Registry, Resources, Consumer,
		EVoxelTerrainResourcePool::Staged, Decoded, Error));
	TestTrue(TEXT("失败保留已完成输出"), Decoded == Previous);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelEmergeCapacityContinuationTest,
	"WHFramework.Voxel.Streaming.EmergeCapacityContinuation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEmergeCapacityContinuationTest::RunTest(const FString&)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	const auto Generator = MakeShared<FVoxelGenerationPipeline, ESPMode::ThreadSafe>(Config, Cache);
	const auto Registry = VoxelTest::MakeRegistry();
	FVoxelTaskScheduler Scheduler(true);
	if (!Scheduler.InitializeStatistics(11)) return false;
	const auto Resources = Scheduler.GetRuntimeResources();
	FVoxelWorldRuntime Runtime(11, true, Registry, Generator);
	Runtime.BindTerrainResources(Resources);
	FVoxelRegionStore Store;
	FVoxelWorldManifest Manifest;
	FVoxelEmergeManager Manager(Runtime, Scheduler, Generator, Config, Cache, Store, Manifest, Registry);
	const FIntVector Key(3, 0, 0);
	auto* Section = Runtime.FindOrAllocate(Key, 1);
	if (!Section) return false;
	FVoxelExactDemand Demand;
	Demand.bExact = true;
	Manager.CurrentDemand.Add(Key, Demand);
	const auto* Waiting = Manager.FindOrCreateWaiting(*Section, Key);
	if (!TestNotNull(TEXT("完成结果保留真实原等待槽"), Waiting)) return false;
	const auto Consumer = Waiting->Consumer;
	FVoxelTaskResult Result;
	Result.Kind = EVoxelTaskKind::GenerateExactBase;
	Result.bSuccess = true;
	Result.Stamp.OwnerId = Manager.GenerationOwnerId;
	Result.Stamp.OwnerIncarnation = Section->Stamp.Token;
	Result.Stamp.WorldEpoch = Runtime.Epoch();
	Result.Stamp.Token = Section->Stamp.Token;
	Result.Stamp.Section = Key;
	Result.Stamp.Revision = Consumer.RequestSerial;
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<TArray<FVoxelBlockState>>() +
		VoxelBoundedAllocation::ArrayBytes<FVoxelBlockState>(4096);
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged, Bytes, Result.OutputResourceLease)) return false;
	Result.BaseBlocks.Init(FVoxelBlockState(), 4096);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ReceiptLease;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::SharedBytes<FVoxelTerrainWorkReceipt>(), ReceiptLease)) return false;
	Result.WorkReceipt = MakeShared<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe>(Scheduler.GetTerrainStatistics().ToSharedRef(), ReceiptLease);
	if (!Result.WorkReceipt->Complete(7.0)) return false;
	const FGuid WorkId = Result.WorkReceipt->GetAttemptId();
	const uint64 Used = Scheduler.GetTerrainStatistics()->GetWorkAccounting().Used;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!FillPool(Resources, EVoxelTerrainResourcePool::Resident, Blocker)) return false;
	TestTrue(TEXT("完成结果由Emerge接收"), Manager.OnTask(MoveTemp(Result)));
	TestTrue(TEXT("容量不足不转Failed"), Section->Status == EVoxelSectionStatus::Allocated && Section->PendingPublish->BaseBlocks.Num() == 4096);
	TestEqual(TEXT("未接纳不计Used"), Scheduler.GetTerrainStatistics()->GetWorkAccounting().Used, Used);
	TestEqual(TEXT("等待保留原执行回执"), Section->PendingPublish->WorkReceipt->GetAttemptId(), WorkId);
	TestFalse(TEXT("未释放容量不能重复消费"), Manager.RequestSection(Key, Demand));
	Blocker.Reset();
	TestTrue(TEXT("容量事件恢复原输出"), Manager.RequestSection(Key, Demand));
	TestTrue(TEXT("恢复后清空预授予完成槽"), Section->PendingPublish->Kind == EVoxelTaskKind::None);
	TestEqual(TEXT("真实接纳才计Used"), Scheduler.GetTerrainStatistics()->GetWorkAccounting().Used, Used + 1);
	const FVoxelSectionStamp Old = Section->Stamp;
	TestTrue(TEXT("无修改Section可退休"), Runtime.RemoveSection(Key));
	Section = Runtime.FindOrAllocate(Key, 2);
	if (!Section) return false;
	FVoxelTaskResult Late;
	Late.Kind = EVoxelTaskKind::GenerateExactBase;
	Late.bSuccess = true;
	Late.Stamp.OwnerId = Manager.GenerationOwnerId;
	Late.Stamp.OwnerIncarnation = Old.Token;
	Late.Stamp.WorldEpoch = Old.Epoch;
	Late.Stamp.Token = Old.Token;
	Late.Stamp.Section = Key;
	Late.Stamp.Revision = Consumer.RequestSerial;
	Late.BaseBlocks.Init(FVoxelBlockState(), 4096);
	Manager.OnTask(MoveTemp(Late));
	TestTrue(TEXT("旧代结果不能写入新代或完成槽"), Section->Status == EVoxelSectionStatus::Allocated &&
		Section->PendingPublish->Kind == EVoxelTaskKind::None && !Section->BaseBlocks);
	const FIntVector CanceledKey(4, 0, 0);
	auto* CanceledSection = Runtime.FindOrAllocate(CanceledKey, 2);
	if (!CanceledSection) return false;
	Manager.CurrentDemand.Add(CanceledKey, Demand);
	const auto* CanceledWaiting = Manager.FindOrCreateWaiting(*CanceledSection, CanceledKey);
	if (!TestNotNull(TEXT("撤销夹具保留真实原等待槽"), CanceledWaiting)) return false;
	const auto CanceledConsumer = CanceledWaiting->Consumer;
	FVoxelTaskResult Canceled;
	Canceled.Kind = EVoxelTaskKind::GenerateExactBase;
	Canceled.bSuccess = true;
	Canceled.Stamp.OwnerId = Manager.GenerationOwnerId;
	Canceled.Stamp.OwnerIncarnation = CanceledSection->Stamp.Token;
	Canceled.Stamp.WorldEpoch = Runtime.Epoch();
	Canceled.Stamp.Token = CanceledSection->Stamp.Token;
	Canceled.Stamp.Section = CanceledKey;
	Canceled.Stamp.Revision = CanceledConsumer.RequestSerial;
	if (!Resources->Acquire(CanceledConsumer, EVoxelTerrainResourcePool::Staged, Bytes, Canceled.OutputResourceLease)) return false;
	Canceled.BaseBlocks.Init(FVoxelBlockState(), 4096);
	ReceiptLease.Reset();
	if (!Resources->Acquire(CanceledConsumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::SharedBytes<FVoxelTerrainWorkReceipt>(), ReceiptLease)) return false;
	Canceled.WorkReceipt = MakeShared<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe>(Scheduler.GetTerrainStatistics().ToSharedRef(), ReceiptLease);
	if (!Canceled.WorkReceipt->Complete(3.0)) return false;
	const uint64 Discarded = Scheduler.GetTerrainStatistics()->GetWorkAccounting().Discarded;
	if (!FillPool(Resources, EVoxelTerrainResourcePool::Resident, Blocker)) return false;
	Manager.OnTask(MoveTemp(Canceled));
	TestTrue(TEXT("撤销前输出完整保留在完成槽"), CanceledSection->PendingPublish->WorkReceipt.IsValid());
	TestTrue(TEXT("无需求的容量等待输出可以退休"), Runtime.RemoveSection(CanceledKey));
	Blocker.Reset();
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FWaitForDiscard>(
		Scheduler.GetTerrainStatistics().ToSharedRef(), Discarded + 1, *this));
	return true;
}

#endif
