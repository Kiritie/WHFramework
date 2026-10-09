#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Network/VoxelOverlayReplica.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

namespace VoxelOverlayReplicaTest
{
	struct FFixture
	{
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
		FVoxelOverlayReplica Replica{1, Resources};
		TArray<FVoxelTaskRequest> Tasks;
		FGuid Subscription = FGuid::NewGuid();
		FGuid Ticket = FGuid::NewGuid();
		FGuid Directory = FGuid::NewGuid();
		FGuid Snapshot = FGuid::NewGuid();
		FVoxelOverlayHash Manifest;
		FVoxelOverlayHash EmptyDirectory;
		FVoxelOverlayHash EmptySnapshot;
		FString Error;
		TArray<FIntVector> Regions{FIntVector(-1, 0, 0), FIntVector(0, 0, 0)};

		explicit FFixture(const FVoxelTerrainResourceLimits& InLimits = {})
		{
			Resources->Configure(InLimits);
			const uint8 Identity[] = {1, 2, 3};
			FVoxelOverlayHash::Compute(MakeArrayView(Identity), Manifest);
			FVoxelOverlayRegionCodec::DirectoryHash({}, EmptyDirectory);
			FVoxelOverlayRegionCodec::SnapshotHash({}, EmptySnapshot);
			Replica.SetAuthority(2, Manifest, Directory);
			Replica.SetTaskBindingDomain(Resources, FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1));
			Replica.SetTaskQueue([this](FVoxelTaskRequest&& Request) { Tasks.Add(MoveTemp(Request)); return true; });
			Replica.ExpectTicket(Subscription, Ticket, Regions);
		}

		FVoxelOverlayRegionMessage Message(const FIntVector& Region) const
		{
			FVoxelOverlayRegionMessage M;
			M.WorldEpoch = 2;
			M.ManifestIdentity = Manifest;
			M.DirectoryEpoch = Directory;
			M.SubscriptionId = Subscription;
			M.CoverageTicket = Ticket;
			M.SnapshotId = Snapshot;
			M.Region = Region;
			M.AuthorityCut = 7;
			M.DirectoryHash = EmptyDirectory;
			M.ContentHash = EmptySnapshot;
			return M;
		}

		bool Send(const EVoxelMessage Kind, FVoxelOverlayRegionMessage&& M)
		{
			TArray<uint8> Wire;
			if (!FVoxelOverlayRegionCodec::Encode(Kind, M, Wire)) { Error = TEXT("测试消息编码失败"); return false; }
			FVoxelOverlayRegionMessage Decoded;
			if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(1, Subscription, 1, M.Region, FPlatformTime::Cycles64() | 1),
				EVoxelTerrainResourcePool::Input, FVoxelOverlayRegionCodec::MaximumMessageBytes * 4, Decoded.ResourceLease)) return false;
			return FVoxelOverlayRegionCodec::Decode(Kind, Wire, Decoded, Error) && Replica.Receive(Kind, MoveTemp(Decoded), Error);
		}

		bool FreezeEnd(FVoxelOverlayRegionMessage& OutMessage)
		{
			auto End = Message(Regions[0]);
			TArray<uint8> Wire;
			if (!FVoxelOverlayRegionCodec::Encode(EVoxelMessage::SnapshotEnd, End, Wire)) return false;
			if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(1, Subscription, 1, End.Region, FPlatformTime::Cycles64() | 1),
				EVoxelTerrainResourcePool::Input, FVoxelOverlayRegionCodec::MaximumMessageBytes * 4, OutMessage.ResourceLease)) return false;
			return FVoxelOverlayRegionCodec::Decode(EVoxelMessage::SnapshotEnd, Wire, OutMessage, Error);
		}

		bool Declare()
		{
			auto M = Message(Regions[0]);
			M.Index = MAX_uint32;
			M.Count = Regions.Num();
			for (const auto& Region : Regions)
			{
				FVoxelOverlayRegionHead Head;
				Head.Region = Region;
				Head.Hash = EmptyDirectory;
				M.Heads.Add(Head);
			}
			return Send(EVoxelMessage::SnapshotBegin, MoveTemp(M));
		}

		bool NaturalRegion(const int32 Index, const uint64 Target)
		{
			auto Begin = Message(Regions[Index]);
			Begin.TargetSequence = Target;
			if (!Send(EVoxelMessage::SnapshotBegin, MoveTemp(Begin))) return false;
			auto End = Message(Regions[Index]);
			End.TargetSequence = Target;
			return Send(EVoxelMessage::SnapshotEnd, MoveTemp(End));
		}

		bool Pump()
		{
			TAtomic<bool> Cancel{false};
			for (int32 Operations = 0; Operations < 32768; ++Operations)
			{
				if (Tasks.IsEmpty())
				{
					if (!Replica.Advance(1, Error)) return false;
					if (Tasks.IsEmpty()) return true;
				}
				auto Task = MoveTemp(Tasks[0]);
				Tasks.RemoveAt(0, 1, EAllowShrinking::No);
				bool bFinished = false;
				for (int32 Step = 0; Step < 32768; ++Step)
				{
					if (Task.GetBlockReason && Task.GetBlockReason() != EVoxelTaskBlockReason::None)
					{
						Error = TEXT("测试资源充足时出现永久等待");
						return false;
					}
					auto Result = Task.Execute(Cancel);
					if (Result.bContinue) continue;
					Result.Kind = Task.Kind;
					Result.Stamp = Task.Stamp;
					if (!Result.bSuccess) { Error = Result.Error; return false; }
					Task.Apply(MoveTemp(Result));
					bFinished = true;
					break;
				}
				if (!bFinished) { Error = TEXT("单个任务没有在有限微步内完成"); return false; }
			}
			Error = TEXT("测试未在有限微步内收敛");
			return false;
		}

		bool Delta()
		{
			TArray<FVoxelOverlayDirectoryEntry> Entries;
			TArray<TArray<uint8>> Payloads;
			auto Header = Message(Regions[0]);
			Header.AuthorityTransaction = FGuid::NewGuid();
			Header.AuthorityCut = 8;
			Header.SnapshotId.Invalidate();
			Header.Index = MAX_uint32;
			Header.Count = 1;
			for (const FIntVector Section : {FIntVector(-1, 0, 0), FIntVector(0, 0, 0)})
			{
				FVoxelOverlaySnapshot Overlay;
				Overlay.Section = Section;
				Overlay.Revision = 1;
				Overlay.Blocks.Add(0, FVoxelBlockState());
				TArray<uint8> Body;
				if (!FVoxelOverlayRegionCodec::EncodeOverlay(Overlay, Body, Error)) return false;
				FVoxelOverlayDirectoryEntry Entry;
				Entry.Section = Section;
				Entry.Revision = 1;
				Entry.PayloadBytes = Body.Num();
				FVoxelOverlayHash::Compute(Body, Entry.PayloadHash);
				FVoxelOverlayRegionHead Head;
				Head.Region = FVoxelOverlayRegionCodec::RegionOf(Section);
				Head.Sequence = 1;
				Head.BeforeHash = EmptyDirectory;
				FVoxelOverlayRegionCodec::DirectoryHash(MakeArrayView(&Entry, 1), Head.Hash);
				Header.Heads.Add(Head);
				Entries.Add(Entry);
				Payloads.Add(MoveTemp(Body));
			}
			TArray<uint8> Body;
			if (!FVoxelOverlayRegionCodec::EncodeDelta(Entries, Payloads, Body)) return false;
			Header.TotalBytes = Body.Num();
			FVoxelOverlayHash::Compute(Body, Header.ContentHash);
			auto Part = Header;
			Part.Index = 0;
			Part.Heads.Reset();
			Part.Payload = MoveTemp(Body);
			// 正文先于 Head 声明到达；两者仍只能形成同一完整事务。
			return Send(EVoxelMessage::RegionDelta, MoveTemp(Part)) && Send(EVoxelMessage::RegionDelta, MoveTemp(Header));
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelOverlaySnapshotAtomicScopeTest,
	"WHFramework.Voxel.FineRefactor.Overlay.AtomicScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelOverlaySnapshotAtomicScopeTest::RunTest(const FString& Parameters)
{
	VoxelOverlayReplicaTest::FFixture F;
	if (!TestTrue(TEXT("声明整组 Ticket"), F.Declare()) || !TestTrue(TEXT("先完成后一个区域"), F.NaturalRegion(1, 0)) ||
		!TestTrue(TEXT("逐区域 Worker 推进"), F.Pump())) { AddError(F.Error); return false; }
	TestTrue(TEXT("区域已准备不能提前交换采样根"), F.Replica.FindRegion(F.Regions[1]) == nullptr);
	if (!TestTrue(TEXT("完成另一个区域"), F.NaturalRegion(0, 0)) || !TestTrue(TEXT("整组统一提交"), F.Pump()))
	{
		AddError(F.Error);
		return false;
	}
	for (const auto& Key : F.Regions)
	{
		TestEqual(TEXT("完整自然目录具有明确水位"), F.Replica.GetState(Key), EVoxelOverlayReplicaState::Complete);
		TestNotNull(TEXT("整组区域均可采样"), F.Replica.FindRegion(Key));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelOverlayCatchupAtomicDeltaTest,
	"WHFramework.Voxel.FineRefactor.Overlay.CatchupAtomicDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelOverlayCatchupAtomicDeltaTest::RunTest(const FString& Parameters)
{
	VoxelOverlayReplicaTest::FFixture F;
	if (!F.Declare() || !F.NaturalRegion(1, 1) || !F.NaturalRegion(0, 1) || !F.Pump()) { AddError(F.Error); return false; }
	for (const auto& Key : F.Regions)
	{
		TestTrue(TEXT("基准目录未追上声明水位时保持私有"), F.Replica.FindRegion(Key) == nullptr);
		TestEqual(TEXT("整组等待连续增量"), F.Replica.GetState(Key), EVoxelOverlayReplicaState::CatchingUp);
	}
	if (!TestTrue(TEXT("跨区域事务允许正文乱序到达"), F.Delta()) || !TestTrue(TEXT("有限微步完成全事务"), F.Pump()))
	{
		AddError(F.Error);
		return false;
	}
	for (const FIntVector Section : {FIntVector(-1, 0, 0), FIntVector(0, 0, 0)})
	{
		uint64 Revision = 0;
		TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> Overlay;
		TestTrue(TEXT("跨区域内容同根可用"), F.Replica.LookupSection(Section, Revision, Overlay));
		TestEqual(TEXT("保留显式 Air 的提交版本"), Revision, uint64(1));
		TestTrue(TEXT("显式 Air 不被当作无编辑"), Overlay && Overlay->Blocks.Contains(0) && Overlay->Blocks.FindChecked(0).IsAir());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelOverlayExpectedTicketNativeTest,
	"WHFramework.Voxel.FineRefactor.Overlay.ExpectedTicketNativeInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelOverlayExpectedTicketNativeTest::RunTest(const FString& InParameters)
{
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	FVoxelTerrainResourceLimits Limits;
	Limits.InputBytes = 1024 * 1024;
	if (!Resources->Configure(Limits)) return false;
	FVoxelOverlayReplica Replica(1, Resources);
	const auto Consumer = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	const auto External = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	const FGuid Subscription = FGuid::NewGuid();
	const FGuid OldTicket = FGuid::NewGuid();
	const FGuid NewTicket = FGuid::NewGuid();
	const FIntVector Regions[] = {{0, 0, 0}, {1, 0, 0}};
	FString Error;
	if (!TestTrue(TEXT("原 Ticket 接纳 native 目录与范围"), Replica.TryExpectTicket(Subscription, OldTicket,
		MakeArrayView(Regions), Consumer, Error) == EVoxelTerrainResourceAdmission::Granted)) return false;
	const uint64 OriginalInput = Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)];
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!Resources->Acquire(External, EVoxelTerrainResourcePool::Input, 1, Blocker)) return false;
	const auto BeforeFill = Resources->Snapshot();
	if (!Blocker->Resize(1 + Limits.InputBytes - BeforeFill.Used[int32(EVoxelTerrainResourcePool::Input)])) return false;
	const uint64 BeforeAttempt = Resources->ReleaseVersionExcludingOwner(Consumer);
	TestTrue(TEXT("新 Ticket 目录输入阻塞"), Replica.TryExpectTicket(Subscription, NewTicket,
		MakeArrayView(Regions), Consumer, Error) == EVoxelTerrainResourceAdmission::CapacityBlocked);
	TestTrue(TEXT("容量不足时旧 Ticket 继续服务"), Replica.TryExpectTicket(Subscription, OldTicket,
		MakeArrayView(Regions), Consumer, Error) == EVoxelTerrainResourceAdmission::Granted);
	TestTrue(TEXT("未发布候选不自唤醒"), Resources->ReleaseVersionExcludingOwner(Consumer) == BeforeAttempt);
	Blocker.Reset();
	TestTrue(TEXT("原 Consumer 真实外部释放后接纳同新 Ticket"), Replica.TryExpectTicket(Subscription, NewTicket,
		MakeArrayView(Regions), Consumer, Error) == EVoxelTerrainResourceAdmission::Granted);
	TestTrue(TEXT("新 Ticket 增加实际 Input backing"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)] > OriginalInput);
	const uint64 BeforeRetire = Resources->ReleaseVersionExcludingOwner(Consumer);
	Replica.CancelTicket(NewTicket);
	TestTrue(TEXT("同 Consumer 已发布 Ticket 退休可唤醒等待"), Resources->ReleaseVersionExcludingOwner(Consumer) > BeforeRetire);
	TestTrue(TEXT("退休释放真实范围保留原目录与旧 Ticket"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)] == OriginalInput);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelOverlayPreparationNativeWaitTest,
	"WHFramework.Voxel.FineRefactor.Overlay.PreparationNativeWait",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelOverlayPreparationNativeWaitTest::RunTest(const FString& InParameters)
{
	VoxelOverlayReplicaTest::FFixture Fixture;
	if (!Fixture.Declare() || !Fixture.Send(EVoxelMessage::SnapshotBegin, Fixture.Message(Fixture.Regions[0]))) return false;
	FVoxelOverlayRegionMessage End;
	if (!Fixture.FreezeEnd(End)) return false;
	const auto External = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!Fixture.Resources->Acquire(External, EVoxelTerrainResourcePool::Input, 1, Blocker)) return false;
	const auto BeforeFill = Fixture.Resources->Snapshot();
	if (!Blocker->Resize(1 + BeforeFill.Limits.InputBytes - BeforeFill.Used[int32(EVoxelTerrainResourcePool::Input)])) return false;
	if (!TestTrue(TEXT("Reliable completion is retained on soft native rejection"),
		Fixture.Replica.Receive(EVoxelMessage::SnapshotEnd, MoveTemp(End), Fixture.Error))) return false;
	TestTrue(TEXT("No worker starts while native Input is occupied"), Fixture.Tasks.IsEmpty() && Fixture.Error.IsEmpty());
	const uint64 FrozenInput = Fixture.Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)];
	for (int32 Retry = 0; Retry < 8; ++Retry)
		if (!Fixture.Replica.Advance(1, Fixture.Error)) return false;
	TestTrue(TEXT("Waiting keeps messages and avoids native reallocation"), Fixture.Tasks.IsEmpty() &&
		Fixture.Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)] == FrozenInput);
	Blocker.Reset();
	if (!Fixture.Replica.Advance(1, Fixture.Error)) { AddError(Fixture.Error); return false; }
	if (!TestEqual(TEXT("The original reliable completion resumes exactly one worker"), Fixture.Tasks.Num(), 1)) return false;
	TestEqual(TEXT("Capacity retries do not consume new attempt serials"), Fixture.Tasks[0].Stamp.Token, uint64(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelOverlayPreparationNativeHardTest,
	"WHFramework.Voxel.FineRefactor.Overlay.PreparationNativeHardFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelOverlayPreparationNativeHardTest::RunTest(const FString& InParameters)
{
	VoxelOverlayReplicaTest::FFixture Fixture;
	if (!Fixture.Declare() || !Fixture.Send(EVoxelMessage::SnapshotBegin, Fixture.Message(Fixture.Regions[0]))) return false;
	FVoxelOverlayRegionMessage End;
	if (!Fixture.FreezeEnd(End)) return false;
	const auto BeforeFill = Fixture.Resources->Snapshot();
	// Fill an input that the original ticket really holds; live ledgers cannot be reconfigured.
	if (!End.ResourceLease->Resize(End.ResourceLease->Bytes() + BeforeFill.Limits.InputBytes -
		BeforeFill.Used[int32(EVoxelTerrainResourcePool::Input)])) return false;
	TestFalse(TEXT("Irreducible native input is an explicit failure"),
		Fixture.Replica.Receive(EVoxelMessage::SnapshotEnd, MoveTemp(End), Fixture.Error));
	TestTrue(TEXT("Hard rejection has a typed reason and resync responsibility"), Fixture.Error.StartsWith(TEXT("CapacityUnachievable:")) &&
		Fixture.Replica.GetState(Fixture.Regions[0]) == EVoxelOverlayReplicaState::ResyncRequired);
	const uint64 FrozenInput = Fixture.Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)];
	TestFalse(TEXT("The same failed reliable input cannot turn into empty success"), Fixture.Replica.Advance(1, Fixture.Error));
	TestTrue(TEXT("Hard rejection preserves reliable messages and starts no worker"), Fixture.Tasks.IsEmpty() &&
		Fixture.Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)] == FrozenInput);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelOverlayWorkerBodyExternalWaitTest,
	"WHFramework.Voxel.FineRefactor.Overlay.WorkerBodyExternalWait",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelOverlayWorkerBodyExternalWaitTest::RunTest(const FString& InParameters)
{
	VoxelOverlayReplicaTest::FFixture Fixture;
	if (!Fixture.Declare() || !Fixture.NaturalRegion(0, 0) || Fixture.Tasks.Num() != 1) return false;
	auto Task = MoveTemp(Fixture.Tasks[0]);
	Fixture.Tasks.Empty();
	const auto External = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!Fixture.Resources->Acquire(External, EVoxelTerrainResourcePool::Working,
		Fixture.Resources->Snapshot().Limits.WorkingBytes, Blocker)) return false;
	TAtomic<bool> Cancel{false};
	auto Result = Task.Execute(Cancel);
	TestTrue(TEXT("Body soft rejection retains the original continuation"), Result.bSuccess && Result.bContinue);
	if (!TestTrue(TEXT("Only an external event can resume the original worker"),
		Task.GetBlockReason() == EVoxelTaskBlockReason::Memory)) return false;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(Task.Stamp.WorldEpoch, Task.Stamp.OwnerId,
		Task.Stamp.OwnerIncarnation, Task.Stamp.Section, Task.Stamp.Token);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> OwnCandidate;
	if (!Fixture.Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident, 1, OwnCandidate)) return false;
	const uint64 BeforeRefund = Fixture.Resources->Snapshot().ReleaseVersion;
	OwnCandidate.Reset();
	TestTrue(TEXT("Own refund advances the global clock but does not wake the blocked body"),
		Fixture.Resources->Snapshot().ReleaseVersion > BeforeRefund && Task.GetBlockReason() == EVoxelTaskBlockReason::Memory);
	Blocker.Reset();
	if (!TestTrue(TEXT("Real external retirement resumes this exact request"), Task.GetBlockReason() == EVoxelTaskBlockReason::None)) return false;
	bool bFinished = false;
	for (int32 Step = 0; Step < 16; ++Step)
	{
		Result = Task.Execute(Cancel);
		if (Result.bContinue) continue;
		if (!Result.bSuccess) { AddError(Result.Error); return false; }
		Result.Kind = Task.Kind; Result.Stamp = Task.Stamp;
		Task.Apply(MoveTemp(Result)); bFinished = true; break;
	}
	TestTrue(TEXT("One body reaches completion without creating a retry worker"), bFinished && Fixture.Tasks.IsEmpty());
	TestTrue(TEXT("The complete scope still keeps its old public root until every region is ready"), Fixture.Replica.FindRegion(Fixture.Regions[0]) == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelOverlayWorkerBodyHardTest,
	"WHFramework.Voxel.FineRefactor.Overlay.WorkerBodyHardFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelOverlayWorkerBodyHardTest::RunTest(const FString& InParameters)
{
	FVoxelTerrainResourceLimits Limits;
	Limits.WorkingBytes = 1;
	VoxelOverlayReplicaTest::FFixture Fixture(Limits);
	if (!Fixture.Declare() || !Fixture.NaturalRegion(0, 0) || Fixture.Tasks.Num() != 1) return false;
	auto Task = MoveTemp(Fixture.Tasks[0]); Fixture.Tasks.Empty();
	TAtomic<bool> Cancel{false};
	auto Result = Task.Execute(Cancel);
	if (!TestTrue(TEXT("Irreducible body backing is terminal rather than an empty continuation"),
		!Result.bSuccess && !Result.bContinue && Result.Error.StartsWith(TEXT("CapacityUnachievable:")))) return false;
	Result.Kind = Task.Kind; Result.Stamp = Task.Stamp;
	Task.Apply(MoveTemp(Result));
	TestTrue(TEXT("The old public root is retained and the original scope requests resync"),
		Fixture.Replica.FindRegion(Fixture.Regions[0]) == nullptr &&
		Fixture.Replica.GetState(Fixture.Regions[0]) == EVoxelOverlayReplicaState::ResyncRequired);
	TestFalse(TEXT("Restored reliable input stays terminal on the original attempt"), Fixture.Replica.Advance(1, Fixture.Error));
	TestTrue(TEXT("Hard body rejection starts no replacement worker"), Fixture.Tasks.IsEmpty() && Fixture.Error.StartsWith(TEXT("CapacityUnachievable:")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelOverlayDeltaBodyHardTest,
	"WHFramework.Voxel.FineRefactor.Overlay.DeltaBodyHardFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelOverlayDeltaBodyHardTest::RunTest(const FString& InParameters)
{
	FVoxelTerrainResourceLimits Limits;
	Limits.WorkingBytes = 256;
	VoxelOverlayReplicaTest::FFixture Fixture(Limits);
	if (!Fixture.Declare() || !Fixture.NaturalRegion(0, 0) || !Fixture.Pump() ||
		!Fixture.NaturalRegion(1, 0) || !Fixture.Pump()) { AddError(Fixture.Error); return false; }
	const auto* Old = Fixture.Replica.FindRegion(Fixture.Regions[0]);
	if (!Old || !Fixture.Delta() || Fixture.Tasks.Num() != 1) return false;
	auto Task = MoveTemp(Fixture.Tasks[0]); Fixture.Tasks.Empty();
	TAtomic<bool> Cancel{false};
	auto Result = Task.Execute(Cancel);
	if (!TestTrue(TEXT("Delta metadata cannot fit the original working limit and terminates"),
		!Result.bSuccess && !Result.bContinue && Result.Error.StartsWith(TEXT("CapacityUnachievable:")))) return false;
	Result.Kind = Task.Kind; Result.Stamp = Task.Stamp; Task.Apply(MoveTemp(Result));
	TestTrue(TEXT("Failed delta keeps the exact old region and reports resync"),
		Fixture.Replica.FindRegion(Fixture.Regions[0]) == Old && Old->Sequence == 0 &&
		Fixture.Replica.GetState(Fixture.Regions[0]) == EVoxelOverlayReplicaState::ResyncRequired);
	TestFalse(TEXT("Received delta remains attached to the same terminal attempt"), Fixture.Replica.Advance(2, Fixture.Error));
	TestTrue(TEXT("No new delta worker or request serial is manufactured"), Fixture.Tasks.IsEmpty());
	return true;
}

#endif
