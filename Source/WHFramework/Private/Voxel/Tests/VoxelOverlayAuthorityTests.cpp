#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Network/VoxelOverlayAuthority.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

namespace VoxelOverlayAuthorityTest
{
	TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> MakeOverlay(
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, const FIntVector& InSection)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Resident,
			VoxelBoundedAllocation::SharedBytes<FVoxelOverlaySnapshot>() +
			VoxelBoundedAllocation::MapBytes<int32, FVoxelBlockState>(4096), Lease)) return nullptr;
		auto Overlay = MakeShared<FVoxelOverlaySnapshot, ESPMode::ThreadSafe>();
		Overlay->Section = InSection;
		Overlay->Revision = 1;
		Overlay->ResourceLease = MoveTemp(Lease);
		Overlay->Blocks.Reserve(4096);
		for (int32 Cell = 0; Cell < 4096; ++Cell) Overlay->Blocks.Add(Cell, FVoxelBlockState());
		return Overlay;
	}

	bool MakeCapture(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> InOverlay,
		FVoxelFineOverlayCapture& OutCapture)
	{
		if (!InOverlay) return false;
		const uint64 Metadata = VoxelBoundedAllocation::SharedBytes<FVoxelOverlaySnapshotSet>() +
			VoxelBoundedAllocation::MapBytes<FIntVector, TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>>(1) +
			VoxelBoundedAllocation::MapBytes<FIntVector, uint64>(1) + FVoxelRegionStore::OverlayReadLeaseBytes(1, 0, 0, 1);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input, Metadata, Lease)) return false;
		FVoxelFineOverlayCapture Capture;
		Capture.WorldEpoch = InConsumer.WorldEpoch;
		Capture.Bounds = {InOverlay->Section * 16, (InOverlay->Section + FIntVector(1)) * 16};
		Capture.Manifest.WorldId = InConsumer.OwnerId;
		Capture.Registry = VoxelTest::MakeRegistry();
		Capture.Resources = InResources;
		Capture.Consumer = InConsumer;
		Capture.Resident.ResourceLease = MoveTemp(Lease);
		Capture.Resident.PinnedSections.Reserve(1);
		Capture.ContentRevisions.Reserve(1);
		Capture.ContentRevisions.Add(InOverlay->Section, InOverlay->Revision);
		Capture.Resident.PinnedSections.Add(InOverlay->Section, MoveTemp(InOverlay));
		Capture.ReadLeaseSet = FVoxelRegionStore::CreateOverlayReadLeaseSet(Capture);
		OutCapture = MoveTemp(Capture);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelOverlayPinnedCaptureTest,
	"WHFramework.Voxel.FineRefactor.Overlay.PinnedCapture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelOverlayPinnedCaptureTest::RunTest(const FString& InParameters)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	Resources->Configure({});
	const auto Consumer = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, FIntVector(-1, 0, 0), 1);
	auto Overlay = VoxelOverlayAuthorityTest::MakeOverlay(Resources, Consumer, Consumer.Min);
	if (!TestTrue(TEXT("创建已收费的不可变提交"), Overlay.IsValid())) return false;
	const auto* Original = Overlay.Get();
	const auto ResidentBefore = Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Resident)];
	FVoxelFineOverlayCapture Capture;
	if (!TestTrue(TEXT("元数据先准入再捕获引用"), VoxelOverlayAuthorityTest::MakeCapture(Resources, Consumer, Overlay, Capture))) return false;
	TestTrue(TEXT("GT 捕获不复制提交正文"), Capture.Resident.Sections.IsEmpty());
	TestTrue(TEXT("借用绑定同一个提交对象"), Capture.Resident.Find(Consumer.Min) == Original);
	FVoxelFineOverlayRead Reader;
	FString Error;
	if (!TestTrue(TEXT("同一切点引用通过验证"), Reader.Initialize(MoveTemp(Capture), Error))) { AddError(Error); return false; }
	FVoxelFineOverlayReadResult Result;
	if (!TestTrue(TEXT("纯驻留引用无需磁盘读取"), Reader.TakeResult(Result))) return false;
	Overlay.Reset();
	TestTrue(TEXT("读取结果保活原提交版本"), Result.Snapshot->Find(Consumer.Min) == Original);
	FVoxelBlockState Cell(1, 0);
	Result.Snapshot->ApplyAt(Consumer.Min * 16, Cell);
	TestTrue(TEXT("借用正文保留显式 Air"), Cell.IsAir());
	TestEqual(TEXT("借用不会再次收费驻留正文"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Resident)], ResidentBefore);
	const TWeakPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> WeakOverlay = Result.Snapshot->PinnedSections.FindChecked(Consumer.Min);
	Result = {};
	if (const auto Retained = WeakOverlay.Pin())
	{
		AddInfo(FString::Printf(TEXT("Overlay retirement diagnostic: references=%d capturePins=%d captureLease=%d readerBytes=%llu"),
			Retained.GetSharedReferenceCount() - 1, Capture.Resident.PinnedSections.Num(), Capture.ReadLeaseSet.IsValid(), Reader.AllocatedBytes()));
	}
	TestEqual(TEXT("最后读取者退出释放提交正文"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Resident)], uint64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelOverlayBoundedSnapshotEncoderTest,
	"WHFramework.Voxel.FineRefactor.Overlay.AuthorityBoundedSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelOverlayBoundedSnapshotEncoderTest::RunTest(const FString& InParameters)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	FVoxelTerrainResourceLimits Limits;
	Limits.StagedBytes = 256 * 1024;
	if (!TestTrue(TEXT("有限发送容量"), Resources->Configure(Limits))) return false;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	FVoxelOverlayAuthorityIdentity Identity;
	Identity.WorldEpoch = 1;
	Identity.Manifest.Bytes[0] = 1;
	Identity.DirectoryEpoch = FGuid::NewGuid();
	Identity.Subscription = FGuid::NewGuid();
	Identity.Ticket = FGuid::NewGuid();
	Identity.Cut = 1;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CaptureLease;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelOverlayAuthorityRegionCapture, FDefaultAllocator>(32), CaptureLease)) return false;
	TArray<FVoxelOverlayAuthorityRegionCapture> Captures;
	Captures.Reserve(32);
	for (int32 Index = 0; Index < 32; ++Index)
	{
		FVoxelOverlayAuthorityRegionCapture Capture;
		Capture.Head.Region = FIntVector(Index, 0, 0);
		Capture.Head.Sequence = 1;
		const auto Overlay = VoxelOverlayAuthorityTest::MakeOverlay(Resources, Consumer, Capture.Head.Region * 8);
		if (!VoxelOverlayAuthorityTest::MakeCapture(Resources, Consumer, Overlay, Capture.Capture)) return false;
		Captures.Add(MoveTemp(Capture));
	}
	TSharedPtr<FVoxelOverlayAuthoritySnapshotBuild, ESPMode::ThreadSafe> Build;
	FString FactoryError;
	if (FVoxelOverlayAuthoritySnapshotBuild::Create(Identity, MoveTemp(Captures), Resources, Consumer, CaptureLease, Build, FactoryError) !=
		EVoxelTerrainResourceAdmission::Granted) { AddError(FactoryError); return false; }
	TAtomic<bool> Cancel{false};
	TSharedPtr<const FVoxelOverlayAuthorityPrepared, ESPMode::ThreadSafe> Snapshot;
	for (int32 Step = 0; Step < 4096; ++Step)
	{
		auto Result = Build->Advance(Cancel);
		if (!Result.bSuccess || Build->IsCapacityBlocked())
		{
			AddError(FString::Printf(TEXT("小容量准备失败或永久等待：%s"), *Result.Error));
			return false;
		}
		if (!Result.bContinue)
		{
			Snapshot = StaticCastSharedPtr<const FVoxelOverlayAuthorityPrepared>(Result.CustomPayload);
			break;
		}
	}
	if (!TestTrue(TEXT("有限微步生成完整目录"), Snapshot.IsValid())) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> HeadLease;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelOverlayRegionHead, FDefaultAllocator>(Snapshot->Bodies.Num()), HeadLease)) return false;
	TArray<FVoxelOverlayRegionHead> Heads;
	Heads.Reserve(Snapshot->Bodies.Num());
	uint64 TotalPayload = 0;
	for (const auto& Body : Snapshot->Bodies)
	{
		Heads.Add(Body.Directory->Head);
		TotalPayload += Body.TotalBytes;
	}
	TestTrue(TEXT("整组正文大于同时可用发送容量"), TotalPayload > Limits.StagedBytes);
	TSharedPtr<FVoxelOverlaySnapshotEncoder, ESPMode::ThreadSafe> Encoder;
	FString EncoderError;
	if (FVoxelOverlaySnapshotEncoder::Create(Snapshot.ToSharedRef(), MoveTemp(Heads), HeadLease, Resources, Consumer,
		Encoder, EncoderError) != EVoxelTerrainResourceAdmission::Granted) { AddError(EncoderError); return false; }
	const auto* PreviousEncoder = Encoder.Get();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ReplacementHeadLease;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelOverlayRegionHead, FDefaultAllocator>(Snapshot->Bodies.Num()), ReplacementHeadLease)) return false;
	TArray<FVoxelOverlayRegionHead> ReplacementHeads;
	ReplacementHeads.Reserve(Snapshot->Bodies.Num());
	for (const auto& Body : Snapshot->Bodies) ReplacementHeads.Add(Body.Directory->Head);
	const auto* FrozenHeads = ReplacementHeads.GetData();
	const auto External = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!Resources->Acquire(External, EVoxelTerrainResourcePool::Input, 1, Blocker)) return false;
	if (!Blocker->Resize(1 + Limits.InputBytes - Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)])) return false;
	const uint64 BeforeAttempt = Resources->ReleaseVersionExcludingOwner(Consumer);
	TestTrue(TEXT("Native factory waits for actual external capacity"), FVoxelOverlaySnapshotEncoder::Create(Snapshot.ToSharedRef(),
		MoveTemp(ReplacementHeads), ReplacementHeadLease, Resources, Consumer, Encoder, EncoderError) == EVoxelTerrainResourceAdmission::CapacityBlocked);
	TestTrue(TEXT("Rejected encoder keeps previous output and frozen head backing"), Encoder.Get() == PreviousEncoder &&
		ReplacementHeads.GetData() == FrozenHeads && ReplacementHeads.Num() == Snapshot->Bodies.Num());
	TestEqual(TEXT("Candidate refund cannot wake its original Consumer"), Resources->ReleaseVersionExcludingOwner(Consumer), BeforeAttempt);
	Blocker.Reset();
	TestTrue(TEXT("External retirement wakes the original attempt"), Resources->ReleaseVersionExcludingOwner(Consumer) > BeforeAttempt);
	if (FVoxelOverlaySnapshotEncoder::Create(Snapshot.ToSharedRef(), MoveTemp(ReplacementHeads), ReplacementHeadLease,
		Resources, Consumer, Encoder, EncoderError) != EVoxelTerrainResourceAdmission::Granted) { AddError(EncoderError); return false; }
	TestTrue(TEXT("Retry adopts heads under the same Consumer"), ReplacementHeads.IsEmpty() && Encoder->GetConsumer() == Consumer);
	int32 CompletedRegions = 0;
	bool bComplete = false;
	for (int32 Step = 0; Step < 4096 && !bComplete; ++Step)
	{
		FVoxelOverlayEncodedMessage Message;
		FString Error;
		if (!Encoder->Advance(Message, bComplete, Error)) { AddError(Error); return false; }
		if (!bComplete && Message.Kind == EVoxelMessage::SnapshotEnd) ++CompletedRegions;
		TestTrue(TEXT("有界消息保持容量上限"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Staged)] <= Limits.StagedBytes);
	}
	TestTrue(TEXT("大快照在小发送容量下完成"), bComplete);
	TestEqual(TEXT("所有区域都有结束声明"), CompletedRegions, 32);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelOverlaySnapshotNativeFactoryTest,
	"WHFramework.Voxel.FineRefactor.Overlay.SnapshotNativeFactory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelOverlaySnapshotNativeFactoryTest::RunTest(const FString& InParameters)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	FVoxelTerrainResourceLimits Limits;
	Limits.InputBytes = 1024 * 1024;
	if (!TestTrue(TEXT("原工厂有限输入配置"), Resources->Configure(Limits))) return false;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	const auto External = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	FVoxelOverlayAuthorityIdentity Identity;
	Identity.WorldEpoch = 1;
	Identity.Manifest.Bytes[0] = 1;
	Identity.DirectoryEpoch = FGuid::NewGuid();
	Identity.Subscription = FGuid::NewGuid();
	Identity.Ticket = FGuid::NewGuid();
	Identity.Cut = 1;
	auto Overlay = VoxelOverlayAuthorityTest::MakeOverlay(Resources, Consumer, {});
	if (!TestTrue(TEXT("捕获保留真实 Resident 输入"), Overlay.IsValid())) return false;
	const auto CaptureInput = [&](TArray<FVoxelOverlayAuthorityRegionCapture>& Captures,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
	{
		const uint64 Bytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelOverlayAuthorityRegionCapture, FDefaultAllocator>(1);
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, Lease)) return false;
		Captures.Reserve(1);
		FVoxelOverlayAuthorityRegionCapture Capture;
		Capture.Head.Sequence = 1;
		if (!VoxelOverlayAuthorityTest::MakeCapture(Resources, Consumer, Overlay, Capture.Capture)) return false;
		Captures.Add(MoveTemp(Capture));
		return true;
	};
	TArray<FVoxelOverlayAuthorityRegionCapture> First;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> FirstLease;
	if (!CaptureInput(First, FirstLease)) return false;
	TSharedPtr<FVoxelOverlayAuthoritySnapshotBuild, ESPMode::ThreadSafe> Build;
	FString Error;
	if (!TestTrue(TEXT("原 native body 接纳"), FVoxelOverlayAuthoritySnapshotBuild::Create(Identity, MoveTemp(First),
		Resources, Consumer, FirstLease, Build, Error) == EVoxelTerrainResourceAdmission::Granted)) return false;
	const auto* Original = Build.Get();
	TArray<FVoxelOverlayAuthorityRegionCapture> Next;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> NextLease;
	if (!CaptureInput(Next, NextLease)) return false;
	const auto* CapturedData = Next.GetData();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!Resources->Acquire(External, EVoxelTerrainResourcePool::Input, 1, Blocker)) return false;
	const auto BeforeFill = Resources->Snapshot();
	if (!Blocker->Resize(1 + Limits.InputBytes - BeforeFill.Used[int32(EVoxelTerrainResourcePool::Input)])) return false;
	const uint64 BeforeAttempt = Resources->ReleaseVersionExcludingOwner(Consumer);
	TestTrue(TEXT("外部占用是可恢复阻塞"), FVoxelOverlayAuthoritySnapshotBuild::Create(Identity, MoveTemp(Next),
		Resources, Consumer, NextLease, Build, Error) == EVoxelTerrainResourceAdmission::CapacityBlocked);
	TestTrue(TEXT("拒收保留旧 factory 输出"), Build.Get() == Original);
	TestTrue(TEXT("拒收不移动原捕获 backing"), Next.GetData() == CapturedData && Next.Num() == 1);
	TestTrue(TEXT("候选退款不会改变原 Consumer 外部事件"), Resources->ReleaseVersionExcludingOwner(Consumer) == BeforeAttempt);
	Blocker.Reset();
	TestTrue(TEXT("真实外部释放恢复原尝试"), Resources->ReleaseVersionExcludingOwner(Consumer) > BeforeAttempt);
	TestTrue(TEXT("同 Consumer 和捕获重试完成"), FVoxelOverlayAuthoritySnapshotBuild::Create(Identity, MoveTemp(Next),
		Resources, Consumer, NextLease, Build, Error) == EVoxelTerrainResourceAdmission::Granted);
	TestTrue(TEXT("接管新 factory 输出并移动原捕获"), Build.Get() != Original && Next.IsEmpty());
	TestTrue(TEXT("原请求 Consumer 没有替换"), Build->GetConsumer() == Consumer);
	return true;
}

#endif
