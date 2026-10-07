#include "Voxel/Network/VoxelOverlayAuthority.h"

#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

namespace
{
	bool LessSection(const FIntVector& A, const FIntVector& B)
	{
		if (A.X != B.X) return A.X < B.X;
		if (A.Y != B.Y) return A.Y < B.Y;
		return A.Z < B.Z;
	}

	bool Grant(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources,
		const FVoxelGenerationConsumerKey& Consumer, EVoxelTerrainResourcePool Pool, uint64 Bytes,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease, FString& Error)
	{
		if (Bytes == 0) { OutLease.Reset(); return true; }
		if (OutLease && OutLease->Resize(Bytes)) return true;
		if (Resources->Acquire(Consumer, Pool, Bytes, OutLease)) return true;
		Error = TEXT("CapacityBlocked: Overlay 协议资源池不足");
		return false;
	}

	TSharedPtr<const FVoxelOverlayWirePayload, ESPMode::ThreadSafe> EncodeOverlay(
		const FVoxelOverlaySnapshot& Overlay, TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources,
		const FVoxelGenerationConsumerKey& Consumer, FString& Error)
	{
		// 编码器的临时编辑数组、正文增长和最终正文同时存在，分别保留。
		uint64 EntityBytes = 0;
		for (const auto& Pair : Overlay.Entities) EntityBytes += VoxelBoundedAllocation::InitialGrowBytes<uint8>(Pair.Value.Payload.Num());
		const uint64 WireBound = 24 + uint64(Overlay.Blocks.Num()) * 7 + uint64(Overlay.Entities.Num()) * 16 + EntityBytes;
		if (WireBound > uint64(FVoxelNetworkCodec::MaxRepresentationBytes))
		{
			Error = TEXT("CapacityUnachievable: 单 Section Overlay 超过协议正文上限");
			return nullptr;
		}
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
		if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Working,
			VoxelBoundedAllocation::InitialGrowBytes<FVoxelSectionCellEdit>(Overlay.Blocks.Num()) +
			VoxelBoundedAllocation::InitialGrowBytes<FVoxelEntityWrite>(Overlay.Entities.Num()) +
			EntityBytes + VoxelBoundedAllocation::InitialGrowBytes<uint8>(WireBound) * 2, Scratch, Error)) return nullptr;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BodyLease;
		if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Staged,
			VoxelBoundedAllocation::SharedBytes<FVoxelOverlayWirePayload>() +
			VoxelBoundedAllocation::InitialGrowBytes<uint8>(WireBound), BodyLease, Error)) return nullptr;
		auto Body = MakeShared<FVoxelOverlayWirePayload, ESPMode::ThreadSafe>();
		Body->ResourceLease = MoveTemp(BodyLease);
		if (!Overlay.Blocks.IsEmpty() || !Overlay.Entities.IsEmpty())
			if (!FVoxelOverlayRegionCodec::EncodeOverlay(Overlay, Body->Bytes, Error)) return nullptr;
		const uint64 Actual = VoxelBoundedAllocation::SharedBytes<FVoxelOverlayWirePayload>() + Body->Bytes.GetAllocatedSize();
		if (Actual > Body->ResourceLease->Bytes()) { Error = TEXT("EstimateMismatch: Overlay 正文容量"); return nullptr; }
		Body->ResourceLease->Resize(Actual);
		return Body;
	}

	FVoxelOverlayRegionMessage BaseMessage(const FVoxelOverlayAuthorityIdentity& Identity)
	{
		FVoxelOverlayRegionMessage Message;
		Message.WorldEpoch = Identity.WorldEpoch;
		Message.ManifestIdentity = Identity.Manifest;
		Message.DirectoryEpoch = Identity.DirectoryEpoch;
		Message.SubscriptionId = Identity.Subscription;
		Message.CoverageTicket = Identity.Ticket;
		Message.AuthorityCut = Identity.Cut;
		return Message;
	}
}

uint64 FVoxelOverlayAuthorityPrepared::GetAllocatedBytes() const
{
	return sizeof(*this) + Bodies.GetAllocatedSize();
}

uint64 FVoxelOverlayAuthorityPrepared::GetResourceAccountedBytes() const
{
	return ResourceLease ? ResourceLease->Bytes() : 0;
}

struct FVoxelOverlayAuthoritySnapshotBuild::FState
{
	FVoxelOverlayAuthorityIdentity Identity;
	TArray<FVoxelOverlayAuthorityRegionCapture> Captures;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CaptureLease;
	FVoxelFineOverlayRead Reader;
	FVoxelFineOverlayReadResult Read;
	TSharedPtr<FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> Root;
	TSharedPtr<FVoxelOverlayAuthorityPrepared, ESPMode::ThreadSafe> Result;
	TSharedPtr<FVoxelOverlayAuthorityDirectory, ESPMode::ThreadSafe> Directory;
	TArray<FIntVector> Sections;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SectionOrderLease;
	int32 RegionCursor = 0;
	int32 SectionCursor = 0;
	bool bReading = false;
	TAtomic<bool> bCapacityBlocked{false};
	TAtomic<uint64> CapacityVersion{0};

	FState(const FVoxelOverlayAuthorityIdentity& InIdentity, TArray<FVoxelOverlayAuthorityRegionCapture>&& InCaptures,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InCaptureLease)
		: Identity(InIdentity), Captures(MoveTemp(InCaptures)), Resources(InResources), Consumer(InConsumer), CaptureLease(MoveTemp(InCaptureLease))
	{
	}

	FVoxelTaskResult Advance(const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Task;
		if (Cancel.Load()) { Task.bCanceled = true; return Task; }
		if (!Result)
		{
			const int32 Count = Captures.Num();
			if (Count == 0 || Count > FVoxelOverlayRegionCodec::MaximumRegions || !CaptureLease)
			{
				Task.Error = TEXT("Overlay 初始捕获为空、超范围或没有输入租约");
				return Task;
			}
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RootLease;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResultLease;
			if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Resident,
				VoxelBoundedAllocation::SharedBytes<FVoxelOverlayAuthorityDirectoryRoot>() +
				VoxelBoundedAllocation::MapBytes<FIntVector, TSharedPtr<const FVoxelOverlayAuthorityDirectory, ESPMode::ThreadSafe>>(Count), RootLease, Task.Error) ||
				!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Staged,
				VoxelBoundedAllocation::SharedBytes<FVoxelOverlayAuthorityPrepared>() +
				VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayAuthorityRegionBody>(Count), ResultLease, Task.Error)) return Block(MoveTemp(Task));
			Root = MakeShared<FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe>();
			Root->Identity = Identity;
			Root->Regions.Reserve(Count);
			Root->ResourceLease = MoveTemp(RootLease);
			Result = MakeShared<FVoxelOverlayAuthorityPrepared, ESPMode::ThreadSafe>();
			Result->Root = Root;
			Result->Bodies.Reserve(Count);
			Result->ResourceLease = MoveTemp(ResultLease);
		}
		if (RegionCursor >= Captures.Num())
		{
			Task.CustomPayload = Result;
			Task.bSuccess = true;
			Captures.Empty();
			CaptureLease.Reset();
			return Task;
		}
		if (!bReading && !Read.Snapshot)
		{
			Reader = FVoxelFineOverlayRead();
			if (!Reader.Initialize(MoveTemp(Captures[RegionCursor].Capture), Task.Error)) return Block(MoveTemp(Task));
			bReading = true;
		}
		if (bReading)
		{
			bool Complete = false;
			if (!Reader.Advance(1, Cancel, Complete, Task.Error)) return Task;
			if (Task.Error.StartsWith(TEXT("CapacityBlocked:"))) return Block(MoveTemp(Task));
			if (!Complete) { Task.bSuccess = Task.bContinue = true; return Task; }
			if (!Reader.TakeResult(Read)) { Task.Error = TEXT("Overlay 固定切点读取结果缺失"); return Task; }
			bReading = false;
		}
		if (!Directory)
		{
			const int32 Count = Read.Snapshot->Num();
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> DirectoryLease;
			if (Count > 512) { Task.Error = TEXT("Region 目录超出 512 个 Section"); return Task; }
			if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Working,
				VoxelBoundedAllocation::ArrayBytes<FIntVector>(Count), SectionOrderLease, Task.Error) ||
				!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Resident,
				VoxelBoundedAllocation::SharedBytes<FVoxelOverlayAuthorityDirectory>() +
				VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayDirectoryEntry>(Count), DirectoryLease, Task.Error)) return Block(MoveTemp(Task));
			Sections.Reserve(Count);
			if (!Read.Snapshot->EnumerateModifiedSections(Read.Bounds, Sections, &Cancel))
			{
				Task.bCanceled = true;
				return Task;
			}
			Sections.Sort(LessSection);
			Directory = MakeShared<FVoxelOverlayAuthorityDirectory, ESPMode::ThreadSafe>();
			Directory->Head = Captures[RegionCursor].Head;
			Directory->Entries.Reserve(Count);
			Directory->ResourceLease = MoveTemp(DirectoryLease);
			auto& Body = Result->Bodies.AddDefaulted_GetRef();
			Body.Directory = Directory;
			Body.Snapshot = Read.Snapshot;
			SectionCursor = 0;
		}
		if (SectionCursor < Sections.Num())
		{
			const auto& Overlay = *Read.Snapshot->Find(Sections[SectionCursor]);
			const auto Payload = EncodeOverlay(Overlay, Resources, Consumer, Task.Error);
			if (!Payload) return Block(MoveTemp(Task));
			FVoxelOverlayDirectoryEntry Entry;
			Entry.Section = Overlay.Section;
			Entry.Revision = Overlay.Revision;
			Entry.bTombstone = Overlay.Blocks.IsEmpty() && Overlay.Entities.IsEmpty();
			Entry.PayloadBytes = Payload->Bytes.Num();
			if (Entry.Revision == 0 || !FVoxelOverlayHash::Compute(Payload->Bytes, Entry.PayloadHash))
			{
				Task.Error = TEXT("Overlay 目录版本或正文摘要无效");
				return Task;
			}
			Directory->Entries.Add(Entry);
			auto& Body = Result->Bodies.Last();
			Body.TotalBytes += Entry.PayloadBytes;
			if (Body.TotalBytes > FVoxelOverlayRegionCodec::MaximumSnapshotBytes)
			{
				Task.Error = TEXT("CapacityUnachievable: Region 快照超过协议上限");
				return Task;
			}
			++SectionCursor;
			Task.bSuccess = Task.bContinue = true;
			return Task;
		}
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> HashScratch;
		if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Working,
			VoxelBoundedAllocation::InitialGrowBytes<uint8>(uint64(Directory->Entries.Num()) * 128 + 4) * 2, HashScratch, Task.Error)) return Block(MoveTemp(Task));
		if (!FVoxelOverlayRegionCodec::DirectoryHash(Directory->Entries, Directory->Head.Hash) ||
			!FVoxelOverlayRegionCodec::SnapshotHash(Directory->Entries, Directory->SnapshotHash))
		{
			Task.Error = TEXT("Overlay 区域目录排序或摘要不合法");
			return Task;
		}
		Root->Regions.Add(Directory->Head.Region, Directory);
		Directory.Reset();
		Read = FVoxelFineOverlayReadResult();
		Sections.Empty();
		SectionOrderLease.Reset();
		++RegionCursor;
		Task.bSuccess = Task.bContinue = true;
		return Task;
	}

	FVoxelTaskResult Block(FVoxelTaskResult&& Task)
	{
		if (Task.Error.StartsWith(TEXT("CapacityBlocked:")))
		{
			CapacityVersion.Store(Resources->Snapshot().ReleaseVersion);
			bCapacityBlocked.Store(true);
			Task.Error.Reset();
			Task.bSuccess = Task.bContinue = true;
		}
		return MoveTemp(Task);
	}
};

FVoxelOverlayAuthoritySnapshotBuild::FVoxelOverlayAuthoritySnapshotBuild(const FVoxelOverlayAuthorityIdentity& Identity,
	TArray<FVoxelOverlayAuthorityRegionCapture>&& Regions, TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources,
	const FVoxelGenerationConsumerKey& Consumer, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CaptureLease)
	: State(MakeUnique<FState>(Identity, MoveTemp(Regions), Resources, Consumer, MoveTemp(CaptureLease)))
{
}

FVoxelOverlayAuthoritySnapshotBuild::~FVoxelOverlayAuthoritySnapshotBuild() = default;

FVoxelTaskResult FVoxelOverlayAuthoritySnapshotBuild::Advance(const TAtomic<bool>& Cancel)
{
	if (State->bCapacityBlocked.Load() && State->Resources->Snapshot().ReleaseVersion <= State->CapacityVersion.Load())
	{
		FVoxelTaskResult Result;
		Result.bSuccess = Result.bContinue = true;
		return Result;
	}
	State->bCapacityBlocked.Store(false);
	return State->Advance(Cancel);
}

bool FVoxelOverlayAuthoritySnapshotBuild::IsCapacityBlocked() const { return State->bCapacityBlocked.Load(); }
uint64 FVoxelOverlayAuthoritySnapshotBuild::GetCapacityVersion() const { return State->CapacityVersion.Load(); }

FVoxelOverlaySnapshotEncoder::FVoxelOverlaySnapshotEncoder(
	TSharedRef<const FVoxelOverlayAuthorityPrepared, ESPMode::ThreadSafe> InSnapshot,
	TArray<FVoxelOverlayRegionHead>&& InCatchupHeads, TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer)
	: Snapshot(InSnapshot), CatchupHeads(MoveTemp(InCatchupHeads)), Resources(InResources), Consumer(InConsumer)
{
}

bool FVoxelOverlaySnapshotEncoder::Advance(FVoxelOverlayEncodedMessage& OutMessage, bool& Complete, FString& Error)
{
	Complete = false;
	if (!MetadataLease)
		if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Working, sizeof(*this) + CatchupHeads.GetAllocatedSize(), MetadataLease, Error)) return false;
	if (!Snapshot->Root || CatchupHeads.Num() != Snapshot->Bodies.Num() || Snapshot->Bodies.IsEmpty())
	{
		Error = TEXT("Overlay 编码缺少完整捕获或 Catchup 向量");
		return false;
	}
	if (RegionCursor == Snapshot->Bodies.Num()) { Complete = true; return true; }
	const auto& Body = Snapshot->Bodies[RegionCursor];
	const auto& Directory = *Body.Directory;
	if (Phase == 3)
	{
		while (PayloadCursor < Directory.Entries.Num() && Directory.Entries[PayloadCursor].bTombstone) ++PayloadCursor;
		if (PayloadCursor == Directory.Entries.Num()) Phase = 4;
	}
	if (Phase == 3 && !ActivePayload)
	{
		const auto& Entry = Directory.Entries[PayloadCursor];
		const auto* Overlay = Body.Snapshot ? Body.Snapshot->Find(Entry.Section) : nullptr;
		if (!Overlay || Overlay->Revision != Entry.Revision)
		{
			Error = TEXT("Overlay 编码缺少目录所绑定的不可变提交版本");
			return false;
		}
		ActivePayload = EncodeOverlay(*Overlay, Resources, Consumer, Error);
		if (!ActivePayload) return false;
		FVoxelOverlayHash Hash;
		if (ActivePayload->Bytes.Num() != int32(Entry.PayloadBytes) ||
			!FVoxelOverlayHash::Compute(ActivePayload->Bytes, Hash) || Hash != Entry.PayloadHash)
		{
			Error = TEXT("Overlay 重编码正文与固定目录身份不符");
			return false;
		}
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Output;
	const uint64 MessagePeak = VoxelBoundedAllocation::InitialGrowBytes<uint8>(FVoxelOverlayRegionCodec::MaximumMessageBytes);
	if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Working,
		MessagePeak * 3 + VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayRegionHead>(Snapshot->Bodies.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayDirectoryEntry>(FVoxelOverlayRegionCodec::MaximumPageEntries), Scratch, Error) ||
		!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Staged, sizeof(FVoxelOverlayEncodedMessage) + MessagePeak, Output, Error)) return false;
	auto Message = BaseMessage(Snapshot->Root->Identity);
	Message.SnapshotId = SnapshotId;
	Message.Region = Directory.Head.Region;
	Message.BaseSequence = Directory.Head.Sequence;
	Message.TargetSequence = CatchupHeads[RegionCursor].Sequence;
	Message.DirectoryRevision = Directory.Head.Sequence;
	Message.DirectoryHash = Directory.Head.Hash;
	Message.ContentHash = Directory.SnapshotHash;
	EVoxelMessage Kind = EVoxelMessage::SnapshotBegin;
	const int32 PageCount = FMath::DivideAndRoundUp(Directory.Entries.Num(), FVoxelOverlayRegionCodec::MaximumPageEntries);
	uint8 NextPhase = Phase;
	int32 NextPage = DirectoryPage;
	int32 NextPayload = PayloadCursor;
	int32 NextPart = PayloadPart;
	bool AdvanceRegion = false;
	if (Phase == 0)
	{
		Message.Index = MAX_uint32;
		Message.Count = Snapshot->Bodies.Num();
		Message.Heads.Reserve(Snapshot->Bodies.Num());
		for (const auto& Region : Snapshot->Bodies) Message.Heads.Add(Region.Directory->Head);
		NextPhase = 1;
	}
	else if (Phase == 1)
	{
		Message.Count = PageCount;
		Message.EntryCount = Directory.Entries.Num();
		Message.TotalBytes = Body.TotalBytes;
		NextPhase = PageCount > 0 ? 2 : 4;
	}
	else if (Phase == 2)
	{
		Kind = EVoxelMessage::DirectoryPage;
		Message.Index = DirectoryPage;
		Message.Count = PageCount;
		const int32 First = DirectoryPage * FVoxelOverlayRegionCodec::MaximumPageEntries;
		const int32 Count = FMath::Min(FVoxelOverlayRegionCodec::MaximumPageEntries, Directory.Entries.Num() - First);
		Message.Entries.Reserve(Count);
		Message.Entries.Append(Directory.Entries.GetData() + First, Count);
		if (!FVoxelOverlayRegionCodec::DirectoryHash(Message.Entries, Message.ContentHash)) return false;
		++NextPage;
		if (NextPage == PageCount) NextPhase = 3;
	}
	else if (Phase == 3)
	{
		Kind = EVoxelMessage::OverlayPayload;
		const auto& Entry = Directory.Entries[NextPayload];
		const auto& Payload = ActivePayload->Bytes;
		Message.Section = Entry.Section;
		Message.OverlayRevision = Entry.Revision;
		Message.Index = NextPart;
		Message.Count = FMath::DivideAndRoundUp(Payload.Num(), FVoxelOverlayRegionCodec::MaximumPartBytes);
		Message.TotalBytes = Payload.Num();
		Message.ContentHash = Entry.PayloadHash;
		const int32 Offset = NextPart * FVoxelOverlayRegionCodec::MaximumPartBytes;
		const int32 Count = FMath::Min(FVoxelOverlayRegionCodec::MaximumPartBytes, Payload.Num() - Offset);
		Message.Payload.Reserve(Count);
		Message.Payload.Append(Payload.GetData() + Offset, Count);
		if (++NextPart == int32(Message.Count)) { NextPart = 0; ++NextPayload; }
	}
	else
	{
		Kind = EVoxelMessage::SnapshotEnd;
		AdvanceRegion = true;
		NextPhase = 1;
		NextPage = NextPayload = NextPart = 0;
	}
	TArray<uint8> Bytes;
	if (!FVoxelOverlayRegionCodec::Encode(Kind, Message, Bytes)) { Error = TEXT("Overlay 区域消息编码失败"); return false; }
	if (uint64(Bytes.GetAllocatedSize()) + sizeof(FVoxelOverlayEncodedMessage) > Output->Bytes())
	{
		Error = TEXT("EstimateMismatch: 区域消息输出容量");
		return false;
	}
	Output->Resize(Bytes.GetAllocatedSize() + sizeof(FVoxelOverlayEncodedMessage));
	OutMessage.Kind = Kind;
	OutMessage.Bytes = MoveTemp(Bytes);
	OutMessage.ResourceLease = MoveTemp(Output);
	if (NextPayload != PayloadCursor || AdvanceRegion) ActivePayload.Reset();
	Phase = NextPhase;
	DirectoryPage = NextPage;
	PayloadCursor = NextPayload;
	PayloadPart = NextPart;
	if (AdvanceRegion) ++RegionCursor;
	return true;
}

uint64 FVoxelOverlayAuthorityDelta::GetAllocatedBytes() const
{
	return sizeof(*this) + Heads.GetAllocatedSize();
}

uint64 FVoxelOverlayAuthorityDelta::GetResourceAccountedBytes() const
{
	return ResourceLease ? ResourceLease->Bytes() : 0;
}

struct FVoxelOverlayAuthorityDeltaBuild::FState
{
	TSharedRef<const FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> Basis;
	FVoxelOverlayAuthorityEditCapture Edit;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> Root;
	TSharedPtr<FVoxelOverlayAuthorityDelta, ESPMode::ThreadSafe> Result;
	TArray<FVoxelOverlayDirectoryEntry> Entries;
	TArray<TSharedPtr<const FVoxelOverlayWirePayload, ESPMode::ThreadSafe>> Bodies;
	TMap<FIntVector, TSharedPtr<FVoxelOverlayAuthorityDirectory, ESPMode::ThreadSafe>> Changed;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WorkingLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> EncodingLease;
	TUniquePtr<FVoxelByteWriter> Writer;
	int32 Cursor = 0;
	int32 HeadCursor = 0;
	int32 EncodeCursor = 0;
	uint8 Phase = 0;
	TAtomic<bool> bCapacityBlocked{false};
	TAtomic<uint64> CapacityVersion{0};

	FState(TSharedRef<const FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> InRoot,
		FVoxelOverlayAuthorityEditCapture&& InEdit, TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer)
		: Basis(InRoot), Edit(MoveTemp(InEdit)), Resources(InResources), Consumer(InConsumer)
	{
	}

	FVoxelTaskResult Block(FVoxelTaskResult&& Task)
	{
		if (Task.Error.StartsWith(TEXT("CapacityBlocked:")))
		{
			CapacityVersion.Store(Resources->Snapshot().ReleaseVersion);
			bCapacityBlocked.Store(true);
			Task.Error.Reset();
			Task.bSuccess = Task.bContinue = true;
		}
		return MoveTemp(Task);
	}

	FVoxelTaskResult Advance(const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Task;
		if (Cancel.Load()) { Task.bCanceled = true; return Task; }
		if (!Result)
		{
			if (!Edit.Transaction.IsValid() || Edit.Overlays.IsEmpty() || Edit.Overlays.Num() > 32768 ||
				Edit.Heads.IsEmpty() || Edit.Heads.Num() > FVoxelOverlayRegionCodec::MaximumRegions || !Edit.ResourceLease)
			{
				Task.Error = TEXT("权威增量捕获缺少事务、完整 Head 或输入租约");
				return Task;
			}
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RootLease;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResultLease;
			if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Working,
				VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayDirectoryEntry>(Edit.Overlays.Num()) +
				VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelOverlayWirePayload, ESPMode::ThreadSafe>>(Edit.Overlays.Num()) +
				VoxelBoundedAllocation::MapBytes<FIntVector, TSharedPtr<FVoxelOverlayAuthorityDirectory, ESPMode::ThreadSafe>>(Edit.Heads.Num()), WorkingLease, Task.Error) ||
				!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Resident,
				VoxelBoundedAllocation::SharedBytes<FVoxelOverlayAuthorityDirectoryRoot>() +
				VoxelBoundedAllocation::MapBytes<FIntVector, TSharedPtr<const FVoxelOverlayAuthorityDirectory, ESPMode::ThreadSafe>>(Basis->Regions.Num()), RootLease, Task.Error) ||
				!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Staged,
				VoxelBoundedAllocation::SharedBytes<FVoxelOverlayAuthorityDelta>() +
				VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayRegionHead>(Edit.Heads.Num()), ResultLease, Task.Error)) return Block(MoveTemp(Task));
			Root = MakeShared<FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe>();
			Root->Identity = Basis->Identity;
			Root->Identity.Cut = Edit.Cut;
			Root->ResourceLease = MoveTemp(RootLease);
			Root->Regions.Reserve(Basis->Regions.Num());
			for (const auto& Pair : Basis->Regions) Root->Regions.Add(Pair.Key, Pair.Value);
			Result = MakeShared<FVoxelOverlayAuthorityDelta, ESPMode::ThreadSafe>();
			Result->Root = Root;
			Result->Transaction = Edit.Transaction;
			Result->Cut = Edit.Cut;
			Result->ResourceLease = MoveTemp(ResultLease);
			Result->Heads.Reserve(Edit.Heads.Num());
			Entries.Reserve(Edit.Overlays.Num());
			Bodies.Reserve(Edit.Overlays.Num());
			Changed.Reserve(Edit.Heads.Num());
			for (const auto& Overlay : Edit.Overlays)
				if (!Overlay) { Task.Error = TEXT("权威增量捕获包含空 Section"); return Task; }
			Edit.Overlays.Sort([](const auto& A, const auto& B) { return LessSection(A->Section, B->Section); });
		}
		if (Phase == 0 && Cursor < Edit.Overlays.Num())
		{
			const auto& Overlay = Edit.Overlays[Cursor];
			if (!Overlay || (Cursor > 0 && !LessSection(Edit.Overlays[Cursor - 1]->Section, Overlay->Section)))
			{
				Task.Error = TEXT("权威增量 Section 重复或缺失");
				return Task;
			}
			const FIntVector Key = FVoxelOverlayRegionCodec::RegionOf(Overlay->Section);
			const auto* BeforeRegion = Basis->Regions.Find(Key);
			const auto* Target = Edit.Heads.FindByPredicate([&Key](const auto& H) { return H.Region == Key; });
			if (!BeforeRegion || !Target || Target->Sequence != (*BeforeRegion)->Head.Sequence + 1 ||
				(*BeforeRegion)->Head.Sequence == MAX_uint64)
			{
				Task.Error = TEXT("权威增量 Region 不是连续水位");
				return Task;
			}
			auto* Directory = Changed.Find(Key);
			if (!Directory)
			{
				const int32 LocalCount = 512;
				TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
				if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Resident,
					VoxelBoundedAllocation::SharedBytes<FVoxelOverlayAuthorityDirectory>() +
					VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayDirectoryEntry>(LocalCount), Lease, Task.Error)) return Block(MoveTemp(Task));
				auto Next = MakeShared<FVoxelOverlayAuthorityDirectory, ESPMode::ThreadSafe>();
				Next->ResourceLease = MoveTemp(Lease);
				Next->Head = *Target;
				Next->Head.PreviousSequence = (*BeforeRegion)->Head.Sequence;
				Next->Head.BeforeHash = (*BeforeRegion)->Head.Hash;
				Next->Entries.Reserve(LocalCount);
				Next->Entries.Append((*BeforeRegion)->Entries);
				Changed.Add(Key, MoveTemp(Next));
				Directory = Changed.Find(Key);
			}
			const auto* Previous = (*BeforeRegion)->Entries.FindByPredicate([&Overlay](const auto& E) { return E.Section == Overlay->Section; });
			const uint64 Revision = Previous ? Previous->Revision : 0;
			if (Revision == MAX_uint64 || Overlay->Revision != Revision + 1)
			{
				Task.Error = TEXT("权威增量 Section 不是连续版本");
				return Task;
			}
			const auto Body = EncodeOverlay(*Overlay, Resources, Consumer, Task.Error);
			if (!Body) return Block(MoveTemp(Task));
			FVoxelOverlayDirectoryEntry Entry;
			Entry.Section = Overlay->Section;
			Entry.BeforeRevision = Revision;
			Entry.BeforeHash = Previous ? Previous->PayloadHash : FVoxelOverlayHash();
			Entry.Revision = Overlay->Revision;
			Entry.bTombstone = Overlay->Blocks.IsEmpty() && Overlay->Entities.IsEmpty();
			Entry.PayloadBytes = Body->Bytes.Num();
			if (!FVoxelOverlayHash::Compute(Body->Bytes, Entry.PayloadHash)) return Task;
			if (auto* Old = (*Directory)->Entries.FindByPredicate([&Entry](const auto& E) { return E.Section == Entry.Section; })) *Old = Entry;
			else
			{
				if ((*Directory)->Entries.Num() == 512) { Task.Error = TEXT("Region 增量目录超出 512 个 Section"); return Task; }
				(*Directory)->Entries.Add(Entry);
			}
			Entries.Add(Entry);
			Bodies.Add(Body);
			++Cursor;
			Task.bSuccess = Task.bContinue = true;
			return Task;
		}
		Phase = FMath::Max<uint8>(Phase, 1);
		if (Phase == 1 && HeadCursor < Edit.Heads.Num())
		{
			const auto Key = Edit.Heads[HeadCursor].Region;
			const auto* Directory = Changed.Find(Key);
			if (!Directory) { Task.Error = TEXT("权威增量包含没有 Section 变更的 Region"); return Task; }
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
			if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Working,
				2 * VoxelBoundedAllocation::InitialGrowBytes<uint8>((*Directory)->Entries.Num() * 128 + 4), Scratch, Task.Error)) return Block(MoveTemp(Task));
			(*Directory)->Entries.Sort([](const auto& A, const auto& B) { return LessSection(A.Section, B.Section); });
			if (!FVoxelOverlayRegionCodec::DirectoryHash((*Directory)->Entries, (*Directory)->Head.Hash) ||
				!FVoxelOverlayRegionCodec::SnapshotHash((*Directory)->Entries, (*Directory)->SnapshotHash)) return Task;
			Result->Heads.Add((*Directory)->Head);
			Root->Regions.Add(Key, *Directory);
			++HeadCursor;
			Task.bSuccess = Task.bContinue = true;
			return Task;
		}
		if (!Writer)
		{
			uint64 Bound = 4 + uint64(Entries.Num()) * 101;
			for (const auto& Body : Bodies) Bound += Body->Bytes.Num();
			if (Bound > FVoxelOverlayRegionCodec::MaximumSnapshotBytes)
			{
				Task.Error = TEXT("CapacityUnachievable: 整个权威事务超过协议上限");
				return Task;
			}
			if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Staged,
				VoxelBoundedAllocation::SharedBytes<FVoxelOverlayWirePayload>() +
				VoxelBoundedAllocation::InitialGrowBytes<uint8>(Bound), EncodingLease, Task.Error)) return Block(MoveTemp(Task));
			Writer = MakeUnique<FVoxelByteWriter>(FVoxelOverlayRegionCodec::MaximumSnapshotBytes);
			Writer->U32(Entries.Num());
		}
		if (EncodeCursor < Entries.Num())
		{
			const auto& Entry = Entries[EncodeCursor];
			FVoxelNetworkCodec::WriteVector(*Writer, Entry.Section);
			Writer->U64(Entry.Revision);
			Writer->U8(Entry.bTombstone ? 1 : 0);
			Writer->U32(Entry.PayloadBytes);
			Writer->Raw(MakeArrayView(Entry.PayloadHash.Bytes));
			Writer->U64(Entry.BeforeRevision);
			Writer->Raw(MakeArrayView(Entry.BeforeHash.Bytes));
			Writer->Blob(Bodies[EncodeCursor]->Bytes, FVoxelNetworkCodec::MaxRepresentationBytes);
			++EncodeCursor;
			Task.bSuccess = Task.bContinue = true;
			return Task;
		}
		auto Payload = MakeShared<FVoxelOverlayWirePayload, ESPMode::ThreadSafe>();
		Payload->ResourceLease = MoveTemp(EncodingLease);
		if (!Writer->Finish(Payload->Bytes) || !FVoxelOverlayHash::Compute(Payload->Bytes, Result->Hash)) return Task;
		const uint64 Actual = VoxelBoundedAllocation::SharedBytes<FVoxelOverlayWirePayload>() + Payload->Bytes.GetAllocatedSize();
		if (Actual > Payload->ResourceLease->Bytes()) { Task.Error = TEXT("EstimateMismatch: 整个权威事务编码容量"); return Task; }
		Payload->ResourceLease->Resize(Actual);
		Result->Payload = MoveTemp(Payload);
		Writer.Reset();
		Task.bSuccess = true;
		Task.CustomPayload = Result;
		return Task;
	}
};

FVoxelOverlayAuthorityDeltaBuild::FVoxelOverlayAuthorityDeltaBuild(
	TSharedRef<const FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> Root,
	FVoxelOverlayAuthorityEditCapture&& Edit, TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources,
	const FVoxelGenerationConsumerKey& Consumer)
	: State(MakeUnique<FState>(Root, MoveTemp(Edit), Resources, Consumer))
{
}

FVoxelOverlayAuthorityDeltaBuild::~FVoxelOverlayAuthorityDeltaBuild() = default;

FVoxelTaskResult FVoxelOverlayAuthorityDeltaBuild::Advance(const TAtomic<bool>& Cancel)
{
	if (Cancel.Load()) { FVoxelTaskResult Result; Result.bCanceled = true; return Result; }
	if (State->bCapacityBlocked.Load() && State->Resources->Snapshot().ReleaseVersion <= State->CapacityVersion.Load())
	{
		FVoxelTaskResult Result;
		Result.bSuccess = Result.bContinue = true;
		return Result;
	}
	State->bCapacityBlocked.Store(false);
	return State->Advance(Cancel);
}

bool FVoxelOverlayAuthorityDeltaBuild::IsCapacityBlocked() const
{
	return State->bCapacityBlocked.Load();
}

uint64 FVoxelOverlayAuthorityDeltaBuild::GetCapacityVersion() const
{
	return State->CapacityVersion.Load();
}

FVoxelOverlayDeltaEncoder::FVoxelOverlayDeltaEncoder(
	TSharedRef<const FVoxelOverlayAuthorityDelta, ESPMode::ThreadSafe> InDelta,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, const FVoxelGenerationConsumerKey& InConsumer)
	: Delta(InDelta), Resources(InResources), Consumer(InConsumer)
{
}

bool FVoxelOverlayDeltaEncoder::Advance(FVoxelOverlayEncodedMessage& OutMessage, bool& Complete, FString& Error)
{
	Complete = false;
	if (!Delta->Root || !Delta->Payload || Delta->Heads.IsEmpty()) { Error = TEXT("增量编码缺少完整事务"); return false; }
	const uint32 Count = FMath::DivideAndRoundUp(Delta->Payload->Bytes.Num(), FVoxelOverlayRegionCodec::MaximumPartBytes);
	if (bHeaderSent && Part == Count) { Complete = true; return true; }
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Output;
	const uint64 MessagePeak = VoxelBoundedAllocation::InitialGrowBytes<uint8>(FVoxelOverlayRegionCodec::MaximumMessageBytes);
	if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Working,
		MessagePeak * 3 + VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayRegionHead>(Delta->Heads.Num()), Scratch, Error) ||
		!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Staged,
		sizeof(FVoxelOverlayEncodedMessage) + MessagePeak, Output, Error)) return false;
	auto Message = BaseMessage(Delta->Root->Identity);
	Message.AuthorityTransaction = Delta->Transaction;
	Message.Region = Delta->Heads[0].Region;
	Message.AuthorityCut = Delta->Cut;
	Message.DirectoryRevision = Delta->Cut;
	Message.Count = Count;
	Message.TotalBytes = Delta->Payload->Bytes.Num();
	Message.ContentHash = Delta->Hash;
	Message.Index = bHeaderSent ? Part : MAX_uint32;
	if (!bHeaderSent)
	{
		Message.Heads.Reserve(Delta->Heads.Num());
		Message.Heads.Append(Delta->Heads);
	}
	else
	{
		const uint32 Offset = Part * FVoxelOverlayRegionCodec::MaximumPartBytes;
		const int32 Bytes = FMath::Min<int32>(FVoxelOverlayRegionCodec::MaximumPartBytes, Message.TotalBytes - Offset);
		Message.Payload.Reserve(Bytes);
		Message.Payload.Append(Delta->Payload->Bytes.GetData() + Offset, Bytes);
	}
	TArray<uint8> Bytes;
	if (!FVoxelOverlayRegionCodec::Encode(EVoxelMessage::RegionDelta, Message, Bytes)) { Error = TEXT("增量消息编码失败"); return false; }
	if (uint64(Bytes.GetAllocatedSize()) + sizeof(FVoxelOverlayEncodedMessage) > Output->Bytes())
	{
		Error = TEXT("EstimateMismatch: 增量消息输出容量");
		return false;
	}
	Output->Resize(Bytes.GetAllocatedSize() + sizeof(FVoxelOverlayEncodedMessage));
	OutMessage.Kind = EVoxelMessage::RegionDelta;
	OutMessage.Bytes = MoveTemp(Bytes);
	OutMessage.ResourceLease = MoveTemp(Output);
	if (bHeaderSent) ++Part;
	else bHeaderSent = true;
	return true;
}
