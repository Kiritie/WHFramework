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

		FFixture()
		{
			Resources->Configure({});
			const uint8 Identity[] = {1, 2, 3};
			FVoxelOverlayHash::Compute(MakeArrayView(Identity), Manifest);
			FVoxelOverlayRegionCodec::DirectoryHash({}, EmptyDirectory);
			FVoxelOverlayRegionCodec::SnapshotHash({}, EmptySnapshot);
			Replica.SetAuthority(2, Manifest, Directory);
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

#endif
