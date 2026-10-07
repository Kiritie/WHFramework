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
	Result = {};
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
	Identity.DirectoryEpoch = FGuid::NewGuid();
	Identity.Subscription = FGuid::NewGuid();
	Identity.Ticket = FGuid::NewGuid();
	Identity.Cut = 1;
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
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CaptureLease;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::SharedBytes<FVoxelOverlayAuthoritySnapshotBuild>() +
		VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayAuthorityRegionCapture>(Captures.Num()), CaptureLease)) return false;
	FVoxelOverlayAuthoritySnapshotBuild Build(Identity, MoveTemp(Captures), Resources, Consumer, MoveTemp(CaptureLease));
	TAtomic<bool> Cancel{false};
	TSharedPtr<const FVoxelOverlayAuthorityPrepared, ESPMode::ThreadSafe> Snapshot;
	for (int32 Step = 0; Step < 4096; ++Step)
	{
		auto Result = Build.Advance(Cancel);
		if (!Result.bSuccess || Build.IsCapacityBlocked())
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
	TArray<FVoxelOverlayRegionHead> Heads;
	uint64 TotalPayload = 0;
	for (const auto& Body : Snapshot->Bodies)
	{
		Heads.Add(Body.Directory->Head);
		TotalPayload += Body.TotalBytes;
	}
	TestTrue(TEXT("整组正文大于同时可用发送容量"), TotalPayload > Limits.StagedBytes);
	FVoxelOverlaySnapshotEncoder Encoder(Snapshot.ToSharedRef(), MoveTemp(Heads), Resources, Consumer);
	int32 CompletedRegions = 0;
	bool bComplete = false;
	for (int32 Step = 0; Step < 4096 && !bComplete; ++Step)
	{
		FVoxelOverlayEncodedMessage Message;
		FString Error;
		if (!Encoder.Advance(Message, bComplete, Error)) { AddError(Error); return false; }
		if (Message.Kind == EVoxelMessage::SnapshotEnd) ++CompletedRegions;
		TestTrue(TEXT("有界消息保持容量上限"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Staged)] <= Limits.StagedBytes);
	}
	TestTrue(TEXT("大快照在小发送容量下完成"), bComplete);
	TestEqual(TEXT("所有区域都有结束声明"), CompletedRegions, 32);
	return true;
}

#endif
