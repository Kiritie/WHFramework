#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Async/Async.h"
#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"
#include "Misc/ScopeExit.h"
#include "HAL/PlatformProcess.h"
#include "EngineGlobals.h"
#include "Voxel/Network/VoxelNetworkEnvelope.h"
#include "Voxel/Network/VoxelModuleNetworkComponent.h"
#include "Voxel/Network/VoxelNetworkTransfer.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Tests/VoxelTestUtilities.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Network/VoxelModuleNetworkOverlayState.h"

namespace VoxelNetworkEnvelopeTest
{
	TSharedPtr<FVoxelNetworkEnvelopeDecode, ESPMode::ThreadSafe> MakeWork(
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources, TArray<uint8>&& Wire,
		const uint32 Crc, FString& Error)
	{
		const auto Consumer = FVoxelGenerationConsumerKey::Section(23, FGuid::NewGuid(), 1, {}, 1);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Input;
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Wire.GetAllocatedSize(), Input)) return nullptr;
		return FVoxelNetworkEnvelopeDecode::Create(Resources, Consumer, MoveTemp(Wire), MoveTemp(Input), Crc, false, Error);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNetworkEnvelopeBoundedCrcTest, "WHFramework.Voxel.FineRefactor.Network.EnvelopeBoundedCrc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNetworkEnvelopeBoundedCrcTest::RunTest(const FString& InParameters)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	Resources->Configure({});
	TArray<uint8> Payload;
	Payload.Init(37, 1024 * 1024);
	const FGuid Session = FGuid::NewGuid();
	TArray<uint8> Wire;
	if (!TestTrue(TEXT("编码大 Envelope"), FVoxelNetworkCodec::Encode(EVoxelMessage::Reply, Session, Payload, Wire))) return false;
	const uint32 Crc = FCrc::MemCrc32(Wire.GetData(), Wire.Num());
	FString Error;
	auto Work = VoxelNetworkEnvelopeTest::MakeWork(Resources, MoveTemp(Wire), Crc, Error);
	if (!TestTrue(TEXT("输入及对象分配前收费"), Work.IsValid())) { AddError(Error); return false; }
	TAtomic<bool> Cancel{false};
	auto First = Async(EAsyncExecution::ThreadPool, [Work, &Cancel] { return Work->Advance(Cancel); }).Get();
	TestTrue(TEXT("首微步不能遍历完整大正文"), First.bSuccess && First.bContinue);
	TestTrue(TEXT("CRC 完成前没有复制 Envelope 正文"), Work->GetMessage().Payload.IsEmpty());
	TestEqual(TEXT("CRC 阶段尚未申请解码工作区"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	bool bComplete = false;
	for (int32 Step = 0; Step < 128; ++Step)
	{
		auto Result = Async(EAsyncExecution::ThreadPool, [Work, &Cancel] { return Work->Advance(Cancel); }).Get();
		if (!Result.bSuccess) { AddError(Result.Error); return false; }
		if (!Result.bContinue) { bComplete = true; break; }
	}
	TestTrue(TEXT("分段 CRC 与完整正文 CRC 一致"), bComplete);
	TestEqual(TEXT("完整会话身份保留"), Work->GetMessage().Session, Session);
	TestEqual(TEXT("大正文无损解码"), Work->GetMessage().Payload, Payload);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNetworkEnvelopeCorruptionTest, "WHFramework.Voxel.FineRefactor.Network.EnvelopeCorruption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNetworkEnvelopeCorruptionTest::RunTest(const FString& InParameters)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	Resources->Configure({});
	TArray<uint8> Payload{1, 2, 3, 4}, Wire;
	FVoxelNetworkCodec::Encode(EVoxelMessage::Reply, FGuid::NewGuid(), Payload, Wire);
	const uint32 Crc = FCrc::MemCrc32(Wire.GetData(), Wire.Num());
	Wire.Last() ^= 1;
	FString Error;
	auto Work = VoxelNetworkEnvelopeTest::MakeWork(Resources, MoveTemp(Wire), Crc, Error);
	if (!Work) { AddError(Error); return false; }
	TAtomic<bool> Cancel{false};
	auto Result = Async(EAsyncExecution::ThreadPool, [Work, &Cancel] { return Work->Advance(Cancel); }).Get();
	TestFalse(TEXT("损坏完整传输正文拒收"), Result.bSuccess);
	TestTrue(TEXT("损坏正文不会生成可应用的 Wire 消息"), Work->GetMessage().Kind == EVoxelMessage::None && !Result.CustomPayload);
	TestEqual(TEXT("CRC 拒收先于解码分配"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNetworkEnvelopeCapacityRecoveryTest, "WHFramework.Voxel.FineRefactor.Network.EnvelopeCapacityRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNetworkEnvelopeCapacityRecoveryTest::RunTest(const FString& InParameters)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	FVoxelTerrainResourceLimits Limits;
	Resources->Configure(Limits);
	FVoxelOverlayRegionMessage M;
	M.WorldEpoch = 29;
	M.SubscriptionId = FGuid::NewGuid(); M.CoverageTicket = FGuid::NewGuid(); M.DirectoryEpoch = FGuid::NewGuid();
	const uint8 Identity[] = {7, 8, 9};
	FVoxelOverlayHash::Compute(MakeArrayView(Identity), M.ManifestIdentity);
	M.Heads.AddDefaulted();
	TArray<uint8> Body, Wire;
	if (!TestTrue(TEXT("编码区域控制正文"), FVoxelOverlayRegionCodec::Encode(EVoxelMessage::RegionAck, M, Body)) ||
		!TestTrue(TEXT("编码区域外层 Envelope"), FVoxelNetworkCodec::Encode(EVoxelMessage::RegionAck, FGuid::NewGuid(), Body, Wire))) return false;
	const uint32 Crc = FCrc::MemCrc32(Wire.GetData(), Wire.Num());
	FString Error;
	auto Work = VoxelNetworkEnvelopeTest::MakeWork(Resources, MoveTemp(Wire), Crc, Error);
	if (!Work) { AddError(Error); return false; }
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	Resources->Acquire(FVoxelGenerationConsumerKey::Section(23, FGuid::NewGuid(), 1, {}, 1),
		EVoxelTerrainResourcePool::Working, Limits.WorkingBytes - 1024, Blocker);
	TAtomic<bool> Cancel{false};
	auto Waiting = Async(EAsyncExecution::ThreadPool, [Work, &Cancel] { return Work->Advance(Cancel); }).Get();
	TestTrue(TEXT("容量不足保留输入并等待"), Waiting.bSuccess && Waiting.bContinue && Work->GetBlockReason() == EVoxelTaskBlockReason::Memory);
	TestTrue(TEXT("等待期间仍未解析区域正文"), Work->GetMessage().Payload.IsEmpty());
	Blocker.Reset();
	TestTrue(TEXT("资源释放后立即恢复可运行"), Work->GetBlockReason() == EVoxelTaskBlockReason::None);
	auto Ready = Async(EAsyncExecution::ThreadPool, [Work, &Cancel] { return Work->Advance(Cancel); }).Get();
	TestTrue(TEXT("同一尝试完成外层和区域正文校验"), Ready.bSuccess && !Ready.bContinue && Work->IsOverlay());
	const auto Overlay = Work->TakeOverlay();
	TestTrue(TEXT("解码后的区域正文持有独立输出租约"), Overlay.ResourceLease.IsValid());
	TestEqual(TEXT("区域会话身份保留"), Overlay.SubscriptionId, M.SubscriptionId);
	TestEqual(TEXT("有界数组解码完整"), Overlay.Heads.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPreparedEnvelopeTransferTest, "WHFramework.Voxel.FineRefactor.Network.PreparedEnvelopeTransfer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPreparedEnvelopeTransferTest::RunTest(const FString& InParameters)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	Resources->Configure({});
	const auto Consumer = FVoxelGenerationConsumerKey::Section(23, FGuid::NewGuid(), 1, {}, 1);
	FVoxelNetworkTransfer Sender;
	FVoxelNetworkTransfer Receiver;
	if (!Sender.BindResources(Resources, Consumer) || !Receiver.BindResources(Resources, Consumer)) return false;
	TArray<uint8> Payload;
	Payload.Init(51, 10 * 1024);
	TArray<uint8> Wire;
	if (!FVoxelNetworkCodec::Encode(EVoxelMessage::Reply, FGuid::NewGuid(), Payload, Wire)) return false;
	const uint32 WrongWorkerCrc = FCrc::MemCrc32(Wire.GetData(), Wire.Num()) ^ 1;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged, Wire.GetAllocatedSize(), Lease)) return false;
	TestTrue(TEXT("GT 接管 Worker 正文及租约"), Sender.EnqueuePrepared(EVoxelTransferPriority::Normal, MoveTemp(Wire), WrongWorkerCrc, Lease));
	bool bRejected = false;
	TArray<uint8> Completed;
	Sender.Tick(FPlatformTime::Seconds(), [&](const TArray<uint8>& Packet)
	{
		bRejected |= !Receiver.Receive(Packet, FPlatformTime::Seconds(), Completed);
	});
	TestEqual(TEXT("有界片段发送完成"), Sender.QueuedBytes(), uint64(0));
	TestTrue(TEXT("接收端拒绝错误 Worker CRC，GT 未重新计算覆盖它"), bRejected && Completed.IsEmpty());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNetworkWorldControlTest,
	"WHFramework.Voxel.FineRefactor.Network.WorldControlOutsideActiveFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNetworkWorldControlTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	auto* Module = NewObject<UVoxelModule>();
	auto* Component = NewObject<UVoxelModuleNetworkComponent>();
	Module->Epoch = 23;
	Module->Generator = VoxelTest::MakeGenerator();
	Module->Runtime = MakeUnique<FVoxelWorldRuntime>(23, false, VoxelTest::MakeRegistry(), Module->Generator.ToSharedRef());
	Module->WorldState = EVoxelWorldState::Running;
	Module->Scheduler = MakeUnique<FVoxelTaskScheduler>();
	Module->Scheduler->SetBudget({});
	Module->Scheduler->BindFrameContext(Module->TerrainFrameContext);
	Component->Module = Module;
	Module->RegisterTerrainNetwork(Component);
	ON_SCOPE_EXIT
	{
		if (Module->TerrainFrameContext.IsActive()) Module->TerrainFrameContext.End(FPlatformTime::Seconds());
		FString CleanupError;
		Module->StopWorld(true, CleanupError);
	};
	const auto Resources = Module->Scheduler->GetTerrainResources();
	Resources->Configure({});
	FString Error;
	if (!Module->TerrainFrameContext.Configure({}, Error)) { AddError(Error); return false; }
	const auto Consumer = FVoxelGenerationConsumerKey::Section(23, Component->TaskOwnerId, Component->TaskOwnerIncarnation, {}, 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> StateLease;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident,
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelOverlayNetworkState>(), StateLease)) return false;
	Component->OverlayNetwork = VoxelBoundedAllocation::MakeWorkerShared<FVoxelOverlayNetworkState>(StateLease);
	Component->OverlayNetwork->Resources = Resources;
	Component->OverlayNetwork->ResourceLease = MoveTemp(StateLease);

	// Envelope 正常，内部 Welcome 无效：必须直到安全控制阶段才报告协议失败。
	const TArray<uint8> Payload{1, 2, 3, 4};
	TArray<uint8> Wire;
	if (!FVoxelNetworkCodec::Encode(EVoxelMessage::Welcome, FGuid::NewGuid(), Payload, Wire)) return false;
	const uint32 Crc = FCrc::MemCrc32(Wire.GetData(), Wire.Num());
	auto Work = VoxelNetworkEnvelopeTest::MakeWork(Resources, MoveTemp(Wire), Crc, Error);
	if (!Work) { AddError(Error); return false; }
	const uint64 Quota = Work->GetIngressQuotaBytes();
	Component->OverlayNetwork->EnvelopeBytes = Quota;
	Component->OverlayNetwork->Envelopes.Enqueue(Work);
	FVoxelTaskScheduler* const Scheduler = Module->Scheduler.Get();
	uint64 FrameCounter = GFrameCounter;
	Module->TerrainFrameContext.Begin(23, ++FrameCounter, FPlatformTime::Seconds(), {});
	Component->AdvanceEnvelopeDecode(FPlatformTime::Seconds());
	if (!TestTrue(TEXT("实际生产入口接纳 Envelope 解码"), Component->OverlayNetwork->bEnvelopeDecode)) return false;
	FVoxelTaskResult WrongAttempt;
	WrongAttempt.Stamp = Component->OverlayNetwork->EnvelopeHeadStamp;
	++WrongAttempt.Stamp.Token;
	WrongAttempt.bSuccess = true;
	TestFalse(TEXT("同 Owner 的旧尝试不能占用完成槽"), Component->StageDecodedEnvelope(Work, MoveTemp(WrongAttempt)));
	for (int32 Step = 0; Step < 512; ++Step)
	{
		Scheduler->Tick([this](FVoxelTaskResult&&) { AddError(TEXT("网络结果不应进入默认 Module Apply")); }, 1.0);
		if (Component->OverlayNetwork->DecodedEnvelope) break;
		Module->TerrainFrameContext.End(FPlatformTime::Seconds());
		FPlatformProcess::SleepNoStats(0.001f);
		Module->TerrainFrameContext.Begin(23, ++FrameCounter, FPlatformTime::Seconds(), {});
	}
	if (!TestTrue(TEXT("实际 Scheduler 完成后只保存结果"), Component->OverlayNetwork->DecodedEnvelope.IsValid())) return false;
	TestFalse(TEXT("Scheduler Apply 尚未执行 Welcome"), Component->bRejected);
	TestTrue(TEXT("世界与 Scheduler 保持存活"), Module->IsReady() && Module->Scheduler.Get() == Scheduler);
	TestTrue(TEXT("控制槽由完成事件入队"), Module->PendingTerrainWorldControl.Get() == Component);
	TestEqual(TEXT("控制处理前连接配额仍持有完整正文"), Component->OverlayNetwork->EnvelopeBytes, Quota);
	TestFalse(TEXT("普通网络阶段不能越过 Welcome 控制"), Component->ApplyDecodedEnvelope(false));
	TestFalse(TEXT("控制阶段拒绝活动帧"), Component->AdvanceDeferredWorldControl());
	TestFalse(TEXT("帧内停止不会销毁调用中的 Scheduler"), Module->StopWorld(true, Error));
	TestEqual(TEXT("被拒绝的停止不改变世界 epoch"), Module->Epoch, uint64(23));
	TestTrue(TEXT("被拒绝的停止保留原 Scheduler"), Module->Scheduler.Get() == Scheduler);

	Module->TerrainFrameContext.End(FPlatformTime::Seconds());
	TestTrue(TEXT("帧退出后消费唯一控制结果"), Component->AdvanceDeferredWorldControl());
	TestTrue(TEXT("内部 Welcome 校验在帧外生效"), Component->bRejected);
	TestEqual(TEXT("控制消费后连接配额归还"), Component->OverlayNetwork->EnvelopeBytes, uint64(0));
	TestFalse(TEXT("控制结果只消费一次"), Component->AdvanceDeferredWorldControl());
	const uint64 OldIncarnation = Component->TaskOwnerIncarnation;
	TestTrue(TEXT("帧外世界停止完成"), Module->StopWorld(true, Error));
	TestEqual(TEXT("世界停止撤销旧网络代"), Component->TaskOwnerIncarnation, OldIncarnation + 1);
	TestFalse(TEXT("世界停止清理旧连接"), Component->OverlayNetwork.IsValid());
	TestFalse(TEXT("世界停止撤销模块控制槽"), Module->PendingTerrainWorldControl.IsValid());
	TestFalse(TEXT("旧会话不再就绪"), Component->IsSessionReady());
	TestEqual(TEXT("外部冻结正文仍可读，退休不悬空"), Work->GetMessage().Payload, Payload);
	FVoxelTaskResult Late;
	Late.Stamp.WorldEpoch = 23;
	Late.Stamp.OwnerId = Component->TaskOwnerId;
	Late.Stamp.OwnerIncarnation = OldIncarnation;
	Late.bSuccess = true;
	TestFalse(TEXT("停止后的迟到结果不能恢复旧连接"), Component->StageDecodedEnvelope(Work, MoveTemp(Late)));
	Work.Reset();
	return true;
}
#endif
