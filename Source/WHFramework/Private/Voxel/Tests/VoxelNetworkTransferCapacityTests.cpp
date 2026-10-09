#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelManifestCodec.h"
#include "Voxel/Network/VoxelModuleNetworkOverlayState.h"
#include "Voxel/Network/VoxelNetworkTransfer.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"

namespace VoxelNetworkTransferCapacityTest
{
	FVoxelGenerationConsumerKey Consumer(const uint64 InSerial)
	{
		return FVoxelGenerationConsumerKey::Section(79, FGuid(1, 2, 3, 4), 1, {}, InSerial);
	}

	bool Create(TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& OutResources, FString& OutError)
	{
		FVoxelTerrainResourceLimits Limits;
		Limits.InputBytes = 8 * 1024 * 1024;
		Limits.WorkingBytes = 64 * 1024;
		return FVoxelTerrainResources::Create(Limits, Consumer(1), OutResources, OutError);
	}

	bool FillInput(const TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutBlocker)
	{
		const auto State = InResources->Snapshot();
		if (State.LeaseCount <= 0) return false;
		const uint64 HandleBytes = State.LeaseHandleBytes / uint64(State.LeaseCount);
		const uint64 Available = State.Limits.InputBytes - State.Used[int32(EVoxelTerrainResourcePool::Input)];
		if (Available <= HandleBytes) return false;
		return InResources->Acquire(Consumer(99), EVoxelTerrainResourcePool::Input, Available - HandleBytes, OutBlocker);
	}

	bool GrantBytes(const TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, const EVoxelTerrainResourcePool InPool, const int32 InCount,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease, TArray<uint8>& OutBytes)
	{
		const uint64 Bytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(InCount);
		if (!InResources->Acquire(InConsumer, InPool, Bytes, OutLease)) return false;
		OutBytes.Reserve(InCount);
		OutBytes.SetNumUninitialized(InCount);
		for (int32 Index = 0; Index < InCount; ++Index) OutBytes[Index] = uint8(Index * 31 + 7);
		return VoxelGenerationAllocation::AllocatedBytes(OutBytes) == OutLease->Bytes();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNetworkTransferDirectoryCapacityTest,
	"WHFramework.Voxel.FineRefactor.Network.TransferDirectoryCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNetworkTransferDirectoryCapacityTest::RunTest(const FString& InParameters)
{
	using namespace VoxelNetworkTransferCapacityTest;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!Create(Resources, Error)) { AddError(Error); return false; }
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Seed;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!Resources->Acquire(Consumer(98), EVoxelTerrainResourcePool::Input, 1, Seed) ||
		!FillInput(Resources.ToSharedRef(), Blocker)) return false;
	FVoxelNetworkTransfer Transfer;
	const auto Original = Consumer(2);
	const uint64 Before = Resources->ReleaseVersionExcludingOwner(Original);
	const auto Occupied = Resources->Snapshot();
	TestFalse(TEXT("Input占满时目录在真实Reserve前拒收"), Transfer.BindResources(Resources.ToSharedRef(), Original));
	TestTrue(TEXT("目录拒收属于可恢复容量"), Transfer.GetLastResourceAdmission() == EVoxelTerrainResourceAdmission::CapacityBlocked);
	TestFalse(TEXT("失败不创建目录租约或绑定原账本"), Transfer.GetMetadataLease() || Transfer.GetResources());
	TestEqual(TEXT("失败不增加native句柄"), Resources->Snapshot().LeaseCount, Occupied.LeaseCount);
	TestEqual(TEXT("未接纳候选没有自身退款唤醒"), Resources->ReleaseVersionExcludingOwner(Original), Before);
	Blocker.Reset();
	TestTrue(TEXT("不同consumer释放后同身份恢复"), Transfer.BindResources(Resources.ToSharedRef(), Original));
	TestTrue(TEXT("目录真实授额属于原Input"), Transfer.GetMetadataLease() &&
		Transfer.GetMetadataLease()->IsOwnedBy(*Resources) && Transfer.GetMetadataLease()->GetPool() == EVoxelTerrainResourcePool::Input);
	const auto Existing = Transfer.GetMetadataLease();
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> OtherLedger;
	if (!Create(OtherLedger, Error)) { AddError(Error); return false; }
	TestFalse(TEXT("同World另账本不能重绑既有目录"), Transfer.BindResources(OtherLedger.ToSharedRef(), Original));
	TestTrue(TEXT("拒收保留原账本和租约"), Transfer.GetResources() == Resources && Transfer.GetMetadataLease() == Existing);
	auto OtherWorld = Original;
	++OtherWorld.WorldEpoch;
	TestFalse(TEXT("跨World完整consumer拒收"), Transfer.BindResources(Resources.ToSharedRef(), OtherWorld));
	TestTrue(TEXT("跨World失败保留冻结consumer"), Transfer.GetConsumer() == Original);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WrongPoolLease;
	TArray<uint8> UnacceptedWire;
	if (!GrantBytes(Resources.ToSharedRef(), Original, EVoxelTerrainResourcePool::Input, 16, WrongPoolLease, UnacceptedWire)) return false;
	const auto OriginalLease = WrongPoolLease;
	TestFalse(TEXT("未取得Staged正文的请求拒收"), Transfer.Enqueue(EVoxelTransferPriority::Normal,
		MoveTemp(UnacceptedWire), MoveTemp(WrongPoolLease), &Error));
	TestTrue(TEXT("失败仍保留原正文及其授额，未提前退租"), UnacceptedWire.Num() == 16 && WrongPoolLease == OriginalLease);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNetworkTransferInputRecoveryTest,
	"WHFramework.Voxel.FineRefactor.Network.TransferInputRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNetworkTransferInputRecoveryTest::RunTest(const FString& InParameters)
{
	using namespace VoxelNetworkTransferCapacityTest;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!Create(Resources, Error)) { AddError(Error); return false; }
	FVoxelNetworkTransfer Receiver;
	const auto Original = Consumer(3);
	if (!Receiver.BindResources(Resources.ToSharedRef(), Original)) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PacketLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> OutputLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	TArray<uint8> Packet;
	TArray<uint8> Output;
	const uint8 Body[] = {11, 22, 33, 44, 55};
	constexpr int32 BodyCount = UE_ARRAY_COUNT(Body);
	const uint64 PacketBytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(28 + BodyCount);
	if (!Resources->Acquire(Consumer(4), EVoxelTerrainResourcePool::Input, PacketBytes, PacketLease) ||
		!GrantBytes(Resources.ToSharedRef(), Consumer(5), EVoxelTerrainResourcePool::Input, 3, OutputLease, Output)) return false;
	FVoxelByteWriter Writer(4608);
	Writer.Reserve(28 + BodyCount);
	Writer.U32(0x34475246);
	Writer.U64(1);
	Writer.U16(0);
	Writer.U16(1);
	Writer.U32(BodyCount);
	Writer.U32(FCrc::MemCrc32(Body, BodyCount));
	Writer.Blob(MakeArrayView(Body), 4096);
	if (!Writer.Finish(Packet) || !FillInput(Resources.ToSharedRef(), Blocker)) return false;
	const auto OldOutputLease = OutputLease;
	FVoxelGenerationRequiredSourceScope PacketSource(Resources.Get(), PacketLease.Get());
	FVoxelGenerationRequiredSourceScope OutputSource(Resources.Get(), OldOutputLease.Get());
	uint32 OutputCrc = 12345;
	const uint64 Before = Resources->ReleaseVersionExcludingOwner(Original);
	TestFalse(TEXT("新接收正文在Reserve前拒收"), Receiver.Assemble(Packet, 1.0, Output, OutputCrc, &OutputLease, &Error));
	TestTrue(TEXT("原原因是临时Input阻塞"), Error.StartsWith(TEXT("CapacityBlocked")));
	TestTrue(TEXT("拒收保留旧输出及原租约"), Output.Num() == 3 && Output[0] == 7 && OutputLease == OldOutputLease);
	TestEqual(TEXT("拒收保留旧CRC"), OutputCrc, uint32(12345));
	TestEqual(TEXT("失败不自行唤醒原消费者"), Resources->ReleaseVersionExcludingOwner(Original), Before);
	Blocker.Reset();
	if (!TestTrue(TEXT("外部释放后原片段重试成功"), Receiver.Assemble(Packet, 1.0, Output, OutputCrc, &OutputLease, &Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("最终正文完整且仍在原Input域"), Output.Num() == BodyCount &&
		FMemory::Memcmp(Output.GetData(), Body, BodyCount) == 0 &&
		OutputLease && OutputLease->IsOwnedBy(*Resources) && OutputLease->GetPool() == EVoxelTerrainResourcePool::Input);
	TestEqual(TEXT("完整CRC保持"), OutputCrc, FCrc::MemCrc32(Body, BodyCount));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNetworkTransferWorkspaceCapacityTest,
	"WHFramework.Voxel.FineRefactor.Network.TransferWorkspaceAndFullSpan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNetworkTransferWorkspaceCapacityTest::RunTest(const FString& InParameters)
{
	using namespace VoxelNetworkTransferCapacityTest;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!Create(Resources, Error)) { AddError(Error); return false; }
	FVoxelNetworkSettings Settings;
	Settings.BurstBytes = 4 * 1024 * 1024;
	Settings.BytesPerSecond = 4 * 1024 * 1024;
	FVoxelNetworkTransfer Sender(Settings);
	FVoxelNetworkTransfer Receiver(Settings);
	const auto SenderConsumer = Consumer(6);
	if (!Sender.BindResources(Resources.ToSharedRef(), SenderConsumer) || !Receiver.BindResources(Resources.ToSharedRef(), Consumer(7))) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WireLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CompletedLease;
	TArray<uint8> Wire;
	TArray<uint8> Completed;
	constexpr int32 BodyCount = FVoxelNetworkCodec::MaxWireBytes;
	if (!GrantBytes(Resources.ToSharedRef(), SenderConsumer, EVoxelTerrainResourcePool::Staged, BodyCount, WireLease, Wire) ||
		!Sender.Enqueue(EVoxelTransferPriority::Bulk, MoveTemp(Wire), WireLease, &Error)) return false;
	WireLease.Reset();
	if (!Resources->Acquire(Consumer(99), EVoxelTerrainResourcePool::Working, Resources->Snapshot().Limits.WorkingBytes, Blocker)) return false;
	int32 PacketCount = 0;
	const uint64 Before = Resources->ReleaseVersionExcludingOwner(SenderConsumer);
	Sender.Tick(1.0, [&PacketCount](TArray<uint8>&& InPacket)
	{
		++PacketCount;
	});
	TestEqual(TEXT("Working阻塞不编码或推进原发送正文"), PacketCount, 0);
	TestEqual(TEXT("拒收保持原队列正文"), Sender.QueuedBytes(), uint64(BodyCount));
	TestEqual(TEXT("Working失败不自行退款唤醒"), Resources->ReleaseVersionExcludingOwner(SenderConsumer), Before);
	Blocker.Reset();
	bool bReceiveSucceeded = true;
	for (int32 Step = 0; Step < 128; ++Step)
	{
		Sender.Tick(2.0 + Step * 0.001, [&](TArray<uint8>&& InPacket)
		{
			++PacketCount;
			if (!Receiver.Receive(InPacket, 2.0, Completed, &CompletedLease, &Error)) bReceiveSucceeded = false;
		});
	}
	TestTrue(TEXT("完整合法传输范围含堆分片位图均可消费"), bReceiveSucceeded && Completed.Num() == BodyCount);
	TestEqual(TEXT("真实512片完整发送"), PacketCount, 512);
	TestEqual(TEXT("原发送队列已清空"), Sender.QueuedBytes(), uint64(0));
	TestEqual(TEXT("最后正文先物理释放再归还Staged"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Staged)], uint64(0));
	TestEqual(TEXT("每片scratch物理退出后Working全部归还"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	TestTrue(TEXT("最终输出授额仅保留真实正文，位图已先释放"), CompletedLease &&
		CompletedLease->Bytes() == VoxelGenerationAllocation::AllocatedBytes(Completed));
	if (Completed.Num() == BodyCount)
	{
		for (int32 Index = 0; Index < BodyCount; ++Index)
		{
			if (Completed[Index] != uint8(Index * 31 + 7)) { AddError(TEXT("完整传输正文发生变化")); break; }
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNetworkQueueBackingRetirementTest,
	"WHFramework.Voxel.FineRefactor.Network.QueueBackingRetirement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNetworkQueueBackingRetirementTest::RunTest(const FString& InParameters)
{
	using namespace VoxelNetworkTransferCapacityTest;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!Create(Resources, Error)) { AddError(Error); return false; }
	TVoxelNetworkQueue<FVoxelOverlayQueuedPacket> Queue;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> NodeLease;
	FVoxelOverlayQueuedPacket Packet;
	FVoxelOverlayQueuedPacket Output;
	Packet.Consumer = Consumer(8);
	if (!GrantBytes(Resources.ToSharedRef(), Packet.Consumer, EVoxelTerrainResourcePool::Input, 64,
		Packet.ResourceLease, Packet.Packet.Bytes)) return false;
	TestFalse(TEXT("未授额节点不触发new或移动正文"), Queue.Enqueue(MoveTemp(Packet), nullptr));
	TestTrue(TEXT("节点拒收保留原正文"), Packet.Packet.Bytes.Num() == 64 && Packet.ResourceLease);
	const uint64 NodeBytes = TVoxelNetworkQueue<FVoxelOverlayQueuedPacket>::NodeBytes();
	if (!Resources->Acquire(Packet.Consumer, EVoxelTerrainResourcePool::Input, NodeBytes, NodeLease)) return false;
	if (!Queue.Enqueue(MoveTemp(Packet), MoveTemp(NodeLease))) return false;
	const auto Before = Resources->Snapshot();
	const uint64 HandleBytes = Before.LeaseHandleBytes / uint64(Before.LeaseCount);
	const uint64 BeforeRetirement = Resources->ReleaseVersionExcludingOwner(Consumer(8));
	if (!TestTrue(TEXT("原节点出队成功"), Queue.Dequeue(Output))) return false;
	TestTrue(TEXT("正文引用转入调用方，原队列已空"), Queue.IsEmpty() && Output.Packet.Bytes.Num() == 64 && Output.ResourceLease);
	TestEqual(TEXT("node物理free后精确退node及其nativehandle"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)],
		Before.Used[int32(EVoxelTerrainResourcePool::Input)] - NodeBytes - HandleBytes);
	TestTrue(TEXT("原已发布节点退休可唤醒同consumer"), Resources->ReleaseVersionExcludingOwner(Consumer(8)) > BeforeRetirement);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNetworkManifestCanonicalBooleanTest,
	"WHFramework.Voxel.FineRefactor.Network.ManifestCanonicalBoolean",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNetworkManifestCanonicalBooleanTest::RunTest(const FString& InParameters)
{
	using namespace VoxelNetworkTransferCapacityTest;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!Create(Resources, Error)) { AddError(Error); return false; }
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
	TArray<uint8> Disabled;
	TArray<uint8> Enabled;
	if (!Resources->Acquire(Consumer(9), EVoxelTerrainResourcePool::Working,
		2 * VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(FVoxelManifestCodec::MaxBytes), Scratch)) return false;
	FVoxelWorldManifest Manifest;
	Manifest.WorldId = FGuid(1, 2, 3, 4);
	Manifest.RegistryHash = 1;
	Manifest.RecipeHash = 2;
	Manifest.BaseSampleHash = 3;
	Manifest.Settings.Ecology.Tree.bEnabled = false;
	if (!FVoxelManifestCodec::Encode(Manifest, Disabled)) return false;
	Manifest.Settings.Ecology.Tree.bEnabled = true;
	if (!FVoxelManifestCodec::Encode(Manifest, Enabled)) return false;
	FVoxelWorldManifest Output;
	if (!TestTrue(TEXT("生产canonical编码仍能解码"), FVoxelManifestCodec::Decode(Enabled, Output))) return false;
	int32 Changed = INDEX_NONE;
	for (int32 Index = 0; Index < Enabled.Num(); ++Index)
	{
		if (Enabled[Index] == Disabled[Index]) continue;
		if (Changed != INDEX_NONE) { AddError(TEXT("布尔变更应只改变一个wire字节")); return false; }
		Changed = Index;
	}
	if (Changed == INDEX_NONE) return false;
	Enabled[Changed] = 2;
	Output.WorldId = FGuid(9, 8, 7, 6);
	const FGuid OriginalWorld = Output.WorldId;
	TestFalse(TEXT("非canonical布尔拒收，wire身份不会别名到canonical值"), FVoxelManifestCodec::Decode(Enabled, Output));
	TestEqual(TEXT("失败保持旧Manifest"), Output.WorldId, OriginalWorld);
	return true;
}
#endif
