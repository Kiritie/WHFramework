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
		FVoxelGenerationRequiredCapacity Required;
		if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, OutLease.Get(), Required, Error)) return false;
		const auto Limits = Resources->Snapshot().Limits;
		uint64 PoolLimit = MAX_uint64;
		switch (Pool)
		{
		case EVoxelTerrainResourcePool::Working: PoolLimit = Limits.WorkingBytes; break;
		case EVoxelTerrainResourcePool::Staged: PoolLimit = Limits.StagedBytes; break;
		case EVoxelTerrainResourcePool::Resident: PoolLimit = Limits.ResidentBytes - Limits.OverlapBytes; break;
		case EVoxelTerrainResourcePool::ResidentOverlap: PoolLimit = Limits.OverlapBytes; break;
		default: break;
		}
		const uint64 Retained = Required.Bytes[int32(Pool)];
		if (Retained > PoolLimit || Bytes > PoolLimit - Retained)
		{
			Error = TEXT("CapacityUnachievable: Overlay 原尝试所持池峰值不可达");
			return false;
		}
		const auto Admission = OutLease ? OutLease->TryResize(Bytes,
			Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount) :
			Resources->TryAcquire(Consumer, Pool, Bytes, OutLease, 0,
				Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
		if (Admission == EVoxelTerrainResourceAdmission::Granted) return true;
		Error = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: Overlay 协议资源池不足") :
			Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: Overlay 协议容量不可达") :
			TEXT("InvalidResource: Overlay 原请求资源身份无效");
		return false;
	}

	TSharedPtr<const FVoxelOverlayWirePayload, ESPMode::ThreadSafe> EncodeOverlay(
		const FVoxelOverlaySnapshot& Overlay, TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources,
		const FVoxelGenerationConsumerKey& Consumer, FString& Error)
	{
		if (Overlay.Revision == 0 || Overlay.Blocks.Num() > 4096 || Overlay.Entities.Num() > 4096)
		{
			Error = TEXT("InvalidResource: Overlay Section identity or cell count");
			return nullptr;
		}
		uint64 EntityScratch = 0;
		uint64 EntityWire = 0;
		for (const auto& Pair : Overlay.Blocks) if (Pair.Key < 0 || Pair.Key >= 4096)
		{
			Error = TEXT("InvalidResource: Overlay block cell index");
			return nullptr;
		}
		for (const auto& Pair : Overlay.Entities)
		{
			if (Pair.Key < 0 || Pair.Key >= 4096 || Pair.Value.Payload.Num() > 256 * 1024)
			{
				Error = TEXT("CapacityUnachievable: Overlay entity cell or payload limit");
				return nullptr;
			}
			EntityScratch += VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(Pair.Value.Payload.Num());
			EntityWire += Pair.Value.Payload.Num();
		}
		const bool bHasContent = !Overlay.Blocks.IsEmpty() || !Overlay.Entities.IsEmpty();
		const uint64 WireBytes = bHasContent ? 24 + uint64(Overlay.Blocks.Num()) * 7 + uint64(Overlay.Entities.Num()) * 11 + EntityWire : 0;
		if (WireBytes > uint64(FVoxelNetworkCodec::MaxRepresentationBytes))
		{
			Error = TEXT("CapacityUnachievable: 单 Section Overlay 超过协议正文上限");
			return nullptr;
		}
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
		if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Working,
			VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelSectionCellEdit, FDefaultAllocator>(Overlay.Blocks.Num()) +
			VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelEntityWrite, FDefaultAllocator>(Overlay.Entities.Num()) + EntityScratch,
			Scratch, Error)) return nullptr;
		FVoxelGenerationRequiredSourceScope ScratchSource(&Resources.Get(), Scratch.Get());
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BodyLease;
		if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Staged,
			VoxelBoundedAllocation::SharedBytes<FVoxelOverlayWirePayload>() +
			VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(int32(WireBytes)), BodyLease, Error)) return nullptr;
		auto Body = MakeShared<FVoxelOverlayWirePayload, ESPMode::ThreadSafe>();
		Body->ResourceLease = MoveTemp(BodyLease);
		if (bHasContent && !FVoxelOverlayRegionCodec::EncodeOverlay(Overlay, Body->Bytes, Error)) return nullptr;
		const uint64 Actual = VoxelBoundedAllocation::SharedBytes<FVoxelOverlayWirePayload>() + VoxelGenerationAllocation::AllocatedBytes(Body->Bytes);
		if (Body->Bytes.Num() != int32(WireBytes) || Actual > Body->ResourceLease->Bytes()) { Error = TEXT("EstimateMismatch: Overlay 精确正文容量"); return nullptr; }
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
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> NativeLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CaptureLease;
	FVoxelOverlayAuthorityIdentity Identity;
	TArray<FVoxelOverlayAuthorityRegionCapture> Captures;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	FVoxelFineOverlayRead Reader;
	FVoxelFineOverlayReadResult Read;
	TSharedPtr<FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> Root;
	TSharedPtr<FVoxelOverlayAuthorityPrepared, ESPMode::ThreadSafe> Result;
	TSharedPtr<FVoxelOverlayAuthorityDirectory, ESPMode::ThreadSafe> Directory;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SectionOrderLease;
	TArray<FIntVector> Sections;
	int32 RegionCursor = 0;
	int32 SectionCursor = 0;
	bool bReading = false;
	TAtomic<bool> bCapacityBlocked{false};
	TAtomic<uint64> CapacityVersion{0};
	uint64 BeforeAdmissionVersion = 0;

	FState(TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InNativeLease, const FVoxelOverlayAuthorityIdentity& InIdentity, TArray<FVoxelOverlayAuthorityRegionCapture>&& InCaptures,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InCaptureLease)
		: NativeLease(MoveTemp(InNativeLease)), CaptureLease(MoveTemp(InCaptureLease)), Identity(InIdentity), Captures(MoveTemp(InCaptures)), Resources(InResources), Consumer(InConsumer)
	{
	}

	void VisitSources(FVoxelGenerationRequiredSourceScope::FVisit Visit) const
	{
		const auto Add = [&](const FVoxelTerrainResourceLease* Lease)
		{
			if (Lease && Lease->IsOwnedBy(*Resources)) FVoxelGenerationRequiredSourceScope::VisitLease(Lease, Visit);
		};
		Add(NativeLease.Get()); Add(CaptureLease.Get()); Add(SectionOrderLease.Get());
		for (const auto& Capture : Captures) FVoxelOverlayAuthoritySnapshotBuild::VisitCaptureSources(Capture.Capture, *Resources, Visit);
		Reader.VisitResourceSources(*Resources, Visit);
		if (Read.Snapshot)
		{
			Add(Read.Snapshot->ResourceLease.Get());
			Read.Snapshot->Visit([&](const FIntVector&, const FVoxelOverlaySnapshot& Overlay) { Add(Overlay.ResourceLease.Get()); Add(Overlay.IndexLease.Get()); });
		}
		if (Read.VersionProof) Add(Read.VersionProof->ResourceLease.Get());
		FVoxelRegionStore::VisitOverlayReadLeaseSources(Read.ReadLeaseSet, *Resources, Visit);
		if (Root) FVoxelOverlayAuthorityDeltaBuild::VisitRootSources(*Root, *Resources, Visit);
		if (Result)
		{
			Add(Result->ResourceLease.Get());
			for (const auto& Body : Result->Bodies) if (Body.Snapshot)
			{
				Add(Body.Snapshot->ResourceLease.Get());
				Body.Snapshot->Visit([&](const FIntVector&, const FVoxelOverlaySnapshot& Overlay) { Add(Overlay.ResourceLease.Get()); Add(Overlay.IndexLease.Get()); });
			}
		}
		if (Directory) Add(Directory->ResourceLease.Get());
	}

	FVoxelTaskResult Advance(const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Task;
		if (Cancel.Load()) { Task.bCanceled = true; return Task; }
		BeforeAdmissionVersion = Resources->ReleaseVersionExcludingOwner(Consumer);
		FVoxelGenerationRequiredSourceScope Sources(&Resources.Get(), this,
			[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit) { static_cast<const FState*>(Context)->VisitSources(Visit); });
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
			TMap<FIntVector, TSharedPtr<const FVoxelOverlayAuthorityDirectory, ESPMode::ThreadSafe>> EmptyRootRegions;
			FVoxelGenerationRequiredSourceScope PendingRootSource(&Resources.Get(), &RootLease,
				[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
				{
					FVoxelGenerationRequiredSourceScope::VisitLease(static_cast<const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>*>(Context)->Get(), Visit);
				});
			if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Resident,
				VoxelBoundedAllocation::SharedBytes<FVoxelOverlayAuthorityDirectoryRoot>() +
				VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FIntVector, TSharedPtr<const FVoxelOverlayAuthorityDirectory, ESPMode::ThreadSafe>>>(EmptyRootRegions, Count), RootLease, Task.Error) ||
				!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Staged,
				VoxelBoundedAllocation::SharedBytes<FVoxelOverlayAuthorityPrepared>() +
				VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelOverlayAuthorityRegionBody, FDefaultAllocator>(Count), ResultLease, Task.Error)) return Block(MoveTemp(Task));
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
				VoxelGenerationAllocation::Private::ArrayReserveBytes<FIntVector, FDefaultAllocator>(Count), SectionOrderLease, Task.Error) ||
				!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Resident,
				VoxelBoundedAllocation::SharedBytes<FVoxelOverlayAuthorityDirectory>() +
				VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelOverlayDirectoryEntry, FDefaultAllocator>(Count), DirectoryLease, Task.Error)) return Block(MoveTemp(Task));
			Sections.Reserve(Count);
			if (!Read.Snapshot->VisitModifiedOverlays(Read.Bounds,
				[this](const FIntVector& Section, const FVoxelOverlaySnapshot&)
				{
					check(Sections.Num() < Sections.Max());
					Sections.Add(Section);
					return true;
				}, Task.Error, &Cancel))
			{
				Task.bCanceled = Cancel.Load();
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
			CapacityVersion.Store(BeforeAdmissionVersion);
			bCapacityBlocked.Store(true);
			Task.Error.Reset();
			Task.bSuccess = Task.bContinue = true;
		}
		return MoveTemp(Task);
	}
};

uint64 FVoxelOverlayAuthoritySnapshotBuild::NativeInputBytes()
{
	return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelOverlayAuthoritySnapshotBuild>() +
		FMemory::QuantizeSize(sizeof(FState), alignof(FState));
}

void FVoxelOverlayAuthoritySnapshotBuild::VisitCaptureSources(const FVoxelFineOverlayCapture& Capture,
	const FVoxelTerrainResources& Resources, FVoxelGenerationRequiredSourceScope::FVisit Visit)
{
	Capture.VisitResourceSources(Resources, Visit);
}

EVoxelTerrainResourceAdmission FVoxelOverlayAuthoritySnapshotBuild::Create(const FVoxelOverlayAuthorityIdentity& Identity,
	TArray<FVoxelOverlayAuthorityRegionCapture>&& Captures, TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources,
	const FVoxelGenerationConsumerKey& Consumer,
	const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& CaptureLease,
	TSharedPtr<FVoxelOverlayAuthoritySnapshotBuild, ESPMode::ThreadSafe>& OutBuild, FString& Error)
{
	if (!Consumer.IsValid() || Consumer.WorldEpoch != Identity.WorldEpoch || Captures.IsEmpty() ||
		Captures.Num() > FVoxelOverlayRegionCodec::MaximumRegions || !CaptureLease || !CaptureLease->IsOwnedBy(*Resources) ||
		CaptureLease->GetPool() != EVoxelTerrainResourcePool::Input ||
		VoxelGenerationAllocation::AllocatedBytes(Captures) > CaptureLease->Bytes())
	{
		Error = TEXT("InvalidResource: Snapshot factory requires original captured input and array grant");
		return EVoxelTerrainResourceAdmission::InvalidRequest;
	}
	for (const auto& Region : Captures)
		if (Region.Capture.WorldEpoch != Consumer.WorldEpoch || !(Region.Capture.Consumer == Consumer) ||
			Region.Capture.Resources.Get() != &Resources.Get() || !Region.Capture.ReadLeaseSet)
		{
			Error = TEXT("InvalidResource: Snapshot capture belongs to a different original consumer or ledger");
			return EVoxelTerrainResourceAdmission::InvalidRequest;
		}
	struct FInput
	{
		const FVoxelTerrainResources& Resources;
		const TArray<FVoxelOverlayAuthorityRegionCapture>& Captures;
		const FVoxelTerrainResourceLease* Lease;
		const FVoxelOverlayAuthoritySnapshotBuild* Previous;
	} Input{*Resources, Captures, CaptureLease.Get(), OutBuild.Get()};
	FVoxelGenerationRequiredSourceScope Sources(&Resources.Get(), &Input,
		[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
		{
			const auto& Input = *static_cast<const FInput*>(Context);
			FVoxelGenerationRequiredSourceScope::VisitLease(Input.Lease, Visit);
			for (const auto& Region : Input.Captures) VisitCaptureSources(Region.Capture, Input.Resources, Visit);
			if (Input.Previous && Input.Previous->NativeLease->IsOwnedBy(Input.Resources)) Input.Previous->State->VisitSources(Visit);
		});
	FVoxelGenerationRequiredCapacity Required;
	if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, Error)) return EVoxelTerrainResourceAdmission::InvalidRequest;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, NativeInputBytes(), Lease, 0,
		Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		Error = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: Snapshot native factory") :
			Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: Snapshot native factory") :
			TEXT("InvalidResource: Snapshot native factory domain");
		return Admission;
	}
	auto Candidate = VoxelBoundedAllocation::MakeWorkerShared<FVoxelOverlayAuthoritySnapshotBuild>(Lease,
		FConstructionKey{}, Lease, Identity, MoveTemp(Captures), Resources, Consumer, CaptureLease);
	OutBuild = MoveTemp(Candidate);
	Error.Reset();
	return EVoxelTerrainResourceAdmission::Granted;
}

FVoxelOverlayAuthoritySnapshotBuild::FVoxelOverlayAuthoritySnapshotBuild(FConstructionKey,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InNativeLease,
	const FVoxelOverlayAuthorityIdentity& Identity,
	TArray<FVoxelOverlayAuthorityRegionCapture>&& Regions, TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources,
	const FVoxelGenerationConsumerKey& Consumer, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CaptureLease)
	: NativeLease(InNativeLease), State(MakeUnique<FState>(MoveTemp(InNativeLease), Identity, MoveTemp(Regions), Resources, Consumer, MoveTemp(CaptureLease)))
{
}

FVoxelOverlayAuthoritySnapshotBuild::~FVoxelOverlayAuthoritySnapshotBuild() = default;

FVoxelTaskResult FVoxelOverlayAuthoritySnapshotBuild::Advance(const TAtomic<bool>& Cancel)
{
	if (Cancel.Load()) { FVoxelTaskResult Result; Result.bCanceled = true; return Result; }
	if (State->bCapacityBlocked.Load() && State->Resources->ReleaseVersionExcludingOwner(State->Consumer) <= State->CapacityVersion.Load())
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
FVoxelGenerationConsumerKey FVoxelOverlayAuthoritySnapshotBuild::GetConsumer() const { return State->Consumer; }

void FVoxelOverlaySnapshotEncoder::VisitSnapshotSources(const FVoxelOverlayAuthorityPrepared& InSnapshot,
	const FVoxelTerrainResources& InResources, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
{
	const auto Add = [&](const FVoxelTerrainResourceLease* Lease)
	{
		if (Lease && Lease->IsOwnedBy(InResources)) FVoxelGenerationRequiredSourceScope::VisitLease(Lease, InVisit);
	};
	Add(InSnapshot.ResourceLease.Get());
	if (InSnapshot.Root) FVoxelOverlayAuthorityDeltaBuild::VisitRootSources(*InSnapshot.Root, InResources, InVisit);
	for (const auto& Body : InSnapshot.Bodies)
	{
		if (Body.Directory) Add(Body.Directory->ResourceLease.Get());
		if (!Body.Snapshot) continue;
		Add(Body.Snapshot->ResourceLease.Get());
		Body.Snapshot->Visit([&](const FIntVector&, const FVoxelOverlaySnapshot& Overlay) { Add(Overlay.ResourceLease.Get()); Add(Overlay.IndexLease.Get()); });
	}
}

void FVoxelOverlaySnapshotEncoder::VisitResourceSources(const FVoxelTerrainResources& InResources,
	FVoxelGenerationRequiredSourceScope::FVisit InVisit) const
{
	VisitSnapshotSources(*Snapshot, InResources, InVisit);
	for (const auto* Lease : {NativeLease.Get(), HeadLease.Get(), ActivePayload ? ActivePayload->ResourceLease.Get() : nullptr})
		if (Lease && Lease->IsOwnedBy(InResources)) FVoxelGenerationRequiredSourceScope::VisitLease(Lease, InVisit);
}

EVoxelTerrainResourceAdmission FVoxelOverlaySnapshotEncoder::Create(
	TSharedRef<const FVoxelOverlayAuthorityPrepared, ESPMode::ThreadSafe> InSnapshot, TArray<FVoxelOverlayRegionHead>&& InCatchupHeads,
	const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InHeadLease,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, const FVoxelGenerationConsumerKey& InConsumer,
	TSharedPtr<FVoxelOverlaySnapshotEncoder, ESPMode::ThreadSafe>& OutEncoder, FString& OutError)
{
	if (!InSnapshot->Root || InSnapshot->Bodies.IsEmpty() || InCatchupHeads.Num() != InSnapshot->Bodies.Num() ||
		InCatchupHeads.Num() > FVoxelOverlayRegionCodec::MaximumRegions || !InConsumer.IsValid() ||
		InConsumer.WorldEpoch != InSnapshot->Root->Identity.WorldEpoch || !InHeadLease || !InHeadLease->IsOwnedBy(*InResources) ||
		InHeadLease->GetPool() != EVoxelTerrainResourcePool::Input || VoxelGenerationAllocation::AllocatedBytes(InCatchupHeads) > InHeadLease->Bytes())
	{
		OutError = TEXT("InvalidResource: Snapshot encoder original input or head grant");
		return EVoxelTerrainResourceAdmission::InvalidRequest;
	}
	for (int32 Index = 0; Index < InSnapshot->Bodies.Num(); ++Index)
		if (!InSnapshot->Bodies[Index].Directory || InCatchupHeads[Index].Region != InSnapshot->Bodies[Index].Directory->Head.Region ||
			InCatchupHeads[Index].Sequence < InSnapshot->Bodies[Index].Directory->Head.Sequence)
		{
			OutError = TEXT("InvalidResource: Snapshot encoder frozen head identity");
			return EVoxelTerrainResourceAdmission::InvalidRequest;
		}
	struct FInput
	{
		const FVoxelTerrainResources& Resources;
		const FVoxelOverlayAuthorityPrepared& Snapshot;
		const FVoxelTerrainResourceLease* Heads;
		const FVoxelOverlaySnapshotEncoder* Previous;
	} Input{*InResources, *InSnapshot, InHeadLease.Get(), OutEncoder.Get()};
	FVoxelGenerationRequiredSourceScope Sources(&InResources.Get(), &Input,
		[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
		{
			const auto& Input = *static_cast<const FInput*>(Context);
			VisitSnapshotSources(Input.Snapshot, Input.Resources, Visit);
			FVoxelGenerationRequiredSourceScope::VisitLease(Input.Heads, Visit);
			if (Input.Previous) Input.Previous->VisitResourceSources(Input.Resources, Visit);
		});
	FVoxelGenerationRequiredCapacity Required;
	if (!FVoxelGenerationRequiredSourceScope::Collect(*InResources, nullptr, Required, OutError)) return EVoxelTerrainResourceAdmission::InvalidRequest;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const auto Admission = InResources->TryAcquire(InConsumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelOverlaySnapshotEncoder>(), Lease, 0, Required.Bytes[0], Required.LeaseCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: Snapshot encoder native factory") :
			Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: Snapshot encoder native factory") :
			TEXT("InvalidResource: Snapshot encoder native factory");
		return Admission;
	}
	OutEncoder = VoxelBoundedAllocation::MakeWorkerShared<FVoxelOverlaySnapshotEncoder>(Lease, FConstructionKey{}, Lease,
		InSnapshot, MoveTemp(InCatchupHeads), InHeadLease, InResources, InConsumer);
	Lease->MarkPublished();
	OutError.Reset();
	return EVoxelTerrainResourceAdmission::Granted;
}

FVoxelOverlaySnapshotEncoder::FVoxelOverlaySnapshotEncoder(FConstructionKey,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InNativeLease,
	TSharedRef<const FVoxelOverlayAuthorityPrepared, ESPMode::ThreadSafe> InSnapshot,
	TArray<FVoxelOverlayRegionHead>&& InCatchupHeads, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InHeadLease,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, const FVoxelGenerationConsumerKey& InConsumer)
	: NativeLease(MoveTemp(InNativeLease)), HeadLease(MoveTemp(InHeadLease)), Snapshot(InSnapshot),
	CatchupHeads(MoveTemp(InCatchupHeads)), Resources(InResources), Consumer(InConsumer)
{
}

bool FVoxelOverlaySnapshotEncoder::Advance(FVoxelOverlayEncodedMessage& OutMessage, bool& Complete, FString& Error)
{
	Complete = false;
	FVoxelGenerationRequiredSourceScope Sources(&Resources.Get(), this,
		[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
		{
			const auto& Encoder = *static_cast<const FVoxelOverlaySnapshotEncoder*>(Context);
			Encoder.VisitResourceSources(*Encoder.Resources, Visit);
		});
	FVoxelGenerationRequiredSourceScope PreviousOutput(&Resources.Get(), OutMessage.ResourceLease.Get());
	if (!Snapshot->Root || CatchupHeads.Num() != Snapshot->Bodies.Num() || Snapshot->Bodies.IsEmpty())
	{
		Error = TEXT("Overlay 编码缺少完整捕获或 Catchup 向量");
		return false;
	}
	if (RegionCursor == Snapshot->Bodies.Num()) { Complete = true; Error.Reset(); return true; }
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
	const int32 HeadCount = Phase == 0 ? Snapshot->Bodies.Num() : 0;
	const int32 EntryCount = Phase == 2 ? FMath::Min(FVoxelOverlayRegionCodec::MaximumPageEntries,
		Directory.Entries.Num() - DirectoryPage * FVoxelOverlayRegionCodec::MaximumPageEntries) : 0;
	const int32 PayloadCount = Phase == 3 ? FMath::Min(FVoxelOverlayRegionCodec::MaximumPartBytes,
		ActivePayload->Bytes.Num() - PayloadPart * FVoxelOverlayRegionCodec::MaximumPartBytes) : 0;
	// DirectoryHash 仍使用 native grow；两份最终 allocator 容量覆盖其真实旧新增长峰值。
	const uint64 HashPeak = EntryCount ? uint64(FMemory::QuantizeSize(VoxelBoundedAllocation::InitialGrowBytes<uint8>(4 + EntryCount * 57), DEFAULT_ALIGNMENT)) * 2 : 0;
	if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Working,
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelOverlayRegionHead, FDefaultAllocator>(HeadCount) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelOverlayDirectoryEntry, FDefaultAllocator>(EntryCount) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(PayloadCount) + HashPeak, Scratch, Error)) return false;
	FVoxelGenerationRequiredSourceScope ScratchSource(&Resources.Get(), Scratch.Get());
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
	int32 EncodedBytes = 0;
	if (!FVoxelOverlayRegionCodec::EncodedByteCount(Message, EncodedBytes)) { Error = TEXT("InvalidResource: Snapshot message encoded count"); return false; }
	if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Staged,
		VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(EncodedBytes), Output, Error)) return false;
	TArray<uint8> Bytes;
	if (!FVoxelOverlayRegionCodec::Encode(Kind, Message, Bytes)) { Error = TEXT("Overlay 区域消息编码失败"); return false; }
	if (Bytes.Num() != EncodedBytes || VoxelGenerationAllocation::AllocatedBytes(Bytes) > Output->Bytes())
	{
		Error = TEXT("EstimateMismatch: 区域消息输出容量");
		return false;
	}
	Output->Resize(VoxelGenerationAllocation::AllocatedBytes(Bytes));
	OutMessage.Kind = Kind;
	OutMessage.Bytes = MoveTemp(Bytes);
	OutMessage.ResourceLease = MoveTemp(Output);
	OutMessage.ResourceLease->MarkPublished();
	if (NextPayload != PayloadCursor || AdvanceRegion) ActivePayload.Reset();
	Phase = NextPhase;
	DirectoryPage = NextPage;
	PayloadCursor = NextPayload;
	PayloadPart = NextPart;
	if (AdvanceRegion) ++RegionCursor;
	Error.Reset();
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
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> NativeLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WorkingLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> EncodingLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WriterLease;
	TSharedRef<const FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> Basis;
	FVoxelOverlayAuthorityEditCapture Edit;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> Root;
	TSharedPtr<FVoxelOverlayAuthorityDelta, ESPMode::ThreadSafe> Result;
	TArray<FVoxelOverlayDirectoryEntry> Entries;
	TArray<TSharedPtr<const FVoxelOverlayWirePayload, ESPMode::ThreadSafe>> Bodies;
	TMap<FIntVector, TSharedPtr<FVoxelOverlayAuthorityDirectory, ESPMode::ThreadSafe>> Changed;
	TUniquePtr<FVoxelByteWriter> Writer;
	int32 Cursor = 0;
	int32 HeadCursor = 0;
	int32 EncodeCursor = 0;
	uint8 Phase = 0;
	TAtomic<bool> bCapacityBlocked{false};
	TAtomic<uint64> CapacityVersion{0};
	uint64 BeforeAdmissionVersion = 0;

	FState(TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InNativeLease, TSharedRef<const FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> InRoot,
		FVoxelOverlayAuthorityEditCapture&& InEdit, TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer)
		: NativeLease(MoveTemp(InNativeLease)), Basis(InRoot), Edit(MoveTemp(InEdit)), Resources(InResources), Consumer(InConsumer)
	{
	}

	FVoxelTaskResult Block(FVoxelTaskResult&& Task)
	{
		if (Task.Error.StartsWith(TEXT("CapacityBlocked:")))
		{
			CapacityVersion.Store(BeforeAdmissionVersion);
			bCapacityBlocked.Store(true);
			Task.Error.Reset();
			Task.bSuccess = Task.bContinue = true;
		}
		return MoveTemp(Task);
	}

	void VisitSources(FVoxelGenerationRequiredSourceScope::FVisit Visit) const
	{
		const auto Add = [&](const FVoxelTerrainResourceLease* Lease)
		{
			if (Lease && Lease->IsOwnedBy(*Resources)) FVoxelGenerationRequiredSourceScope::VisitLease(Lease, Visit);
		};
		Add(NativeLease.Get()); Add(WorkingLease.Get()); Add(EncodingLease.Get()); Add(WriterLease.Get());
		FVoxelOverlayAuthorityDeltaBuild::VisitRootSources(*Basis, *Resources, Visit);
		FVoxelOverlayAuthorityDeltaBuild::VisitEditSources(Edit, *Resources, Visit);
		if (Root) FVoxelOverlayAuthorityDeltaBuild::VisitRootSources(*Root, *Resources, Visit);
		if (Result)
		{
			Add(Result->ResourceLease.Get());
			if (Result->Payload) Add(Result->Payload->ResourceLease.Get());
		}
		for (const auto& Pair : Changed) Add(Pair.Value->ResourceLease.Get());
		for (const auto& Body : Bodies) if (Body) Add(Body->ResourceLease.Get());
	}

	FVoxelTaskResult Advance(const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Task;
		if (Cancel.Load()) { Task.bCanceled = true; return Task; }
		BeforeAdmissionVersion = Resources->ReleaseVersionExcludingOwner(Consumer);
		FVoxelGenerationRequiredSourceScope Sources(&Resources.Get(), this,
			[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit) { static_cast<const FState*>(Context)->VisitSources(Visit); });
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
			TMap<FIntVector, TSharedPtr<const FVoxelOverlayAuthorityDirectory, ESPMode::ThreadSafe>> EmptyRootRegions;
			FVoxelGenerationRequiredSourceScope PendingRootSource(&Resources.Get(), &RootLease,
				[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
				{
					FVoxelGenerationRequiredSourceScope::VisitLease(static_cast<const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>*>(Context)->Get(), Visit);
				});
			if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Working,
				VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelOverlayDirectoryEntry, FDefaultAllocator>(Edit.Overlays.Num()) +
				VoxelGenerationAllocation::Private::ArrayReserveBytes<TSharedPtr<const FVoxelOverlayWirePayload, ESPMode::ThreadSafe>, FDefaultAllocator>(Edit.Overlays.Num()) +
				VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FIntVector, TSharedPtr<FVoxelOverlayAuthorityDirectory, ESPMode::ThreadSafe>>>(Changed, Edit.Heads.Num()), WorkingLease, Task.Error) ||
				!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Resident,
				VoxelBoundedAllocation::SharedBytes<FVoxelOverlayAuthorityDirectoryRoot>() +
				VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FIntVector, TSharedPtr<const FVoxelOverlayAuthorityDirectory, ESPMode::ThreadSafe>>>(EmptyRootRegions, Basis->Regions.Num()), RootLease, Task.Error) ||
				!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Staged,
				VoxelBoundedAllocation::SharedBytes<FVoxelOverlayAuthorityDelta>() +
				VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelOverlayRegionHead, FDefaultAllocator>(Edit.Heads.Num()), ResultLease, Task.Error)) return Block(MoveTemp(Task));
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
					VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelOverlayDirectoryEntry, FDefaultAllocator>(LocalCount), Lease, Task.Error)) return Block(MoveTemp(Task));
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
			if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Working,
				FMemory::QuantizeSize(sizeof(FVoxelByteWriter), alignof(FVoxelByteWriter)), WriterLease, Task.Error) ||
				!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Staged,
					VoxelBoundedAllocation::SharedBytes<FVoxelOverlayWirePayload>() +
					VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(int32(Bound)), EncodingLease, Task.Error)) return Block(MoveTemp(Task));
			Writer = MakeUnique<FVoxelByteWriter>(FVoxelOverlayRegionCodec::MaximumSnapshotBytes);
			if (!Writer->Reserve(int32(Bound))) { Task.Error = TEXT("InvalidResource: 权威事务精确编码容量"); return Task; }
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
		// Finish 拒收时 Writer 仍可能持有 buffer；原编码租约保留到真正转移完成。
		Payload->ResourceLease = EncodingLease;
		if (!Writer->Finish(Payload->Bytes) || !FVoxelOverlayHash::Compute(Payload->Bytes, Result->Hash)) return Task;
		const uint64 Actual = VoxelBoundedAllocation::SharedBytes<FVoxelOverlayWirePayload>() + Payload->Bytes.GetAllocatedSize();
		if (Actual > Payload->ResourceLease->Bytes()) { Task.Error = TEXT("EstimateMismatch: 整个权威事务编码容量"); return Task; }
		Payload->ResourceLease->Resize(Actual);
		Result->Payload = MoveTemp(Payload);
		EncodingLease.Reset();
		Writer.Reset();
		WriterLease.Reset();
		Task.bSuccess = true;
		Task.CustomPayload = Result;
		return Task;
	}
};

uint64 FVoxelOverlayAuthorityDeltaBuild::NativeInputBytes()
{
	return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelOverlayAuthorityDeltaBuild>() +
		FMemory::QuantizeSize(sizeof(FState), alignof(FState));
}

void FVoxelOverlayAuthorityDeltaBuild::VisitRootSources(const FVoxelOverlayAuthorityDirectoryRoot& Root,
	const FVoxelTerrainResources& Resources, FVoxelGenerationRequiredSourceScope::FVisit Visit)
{
	const auto Add = [&](const FVoxelTerrainResourceLease* Lease)
	{
		if (Lease && Lease->IsOwnedBy(Resources)) FVoxelGenerationRequiredSourceScope::VisitLease(Lease, Visit);
	};
	Add(Root.ResourceLease.Get());
	for (const auto& Pair : Root.Regions) if (Pair.Value) Add(Pair.Value->ResourceLease.Get());
}

void FVoxelOverlayAuthorityDeltaBuild::VisitEditSources(const FVoxelOverlayAuthorityEditCapture& Edit,
	const FVoxelTerrainResources& Resources, FVoxelGenerationRequiredSourceScope::FVisit Visit)
{
	const auto Add = [&](const FVoxelTerrainResourceLease* Lease)
	{
		if (Lease && Lease->IsOwnedBy(Resources)) FVoxelGenerationRequiredSourceScope::VisitLease(Lease, Visit);
	};
	Add(Edit.ResourceLease.Get());
	for (const auto& Overlay : Edit.Overlays) if (Overlay) { Add(Overlay->ResourceLease.Get()); Add(Overlay->IndexLease.Get()); }
}

EVoxelTerrainResourceAdmission FVoxelOverlayAuthorityDeltaBuild::Create(
	TSharedRef<const FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> Root,
	FVoxelOverlayAuthorityEditCapture&& Edit, TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources,
	const FVoxelGenerationConsumerKey& Consumer,
	TSharedPtr<FVoxelOverlayAuthorityDeltaBuild, ESPMode::ThreadSafe>& OutBuild, FString& Error)
{
	if (!Consumer.IsValid() || Consumer.WorldEpoch != Root->Identity.WorldEpoch || !Edit.Transaction.IsValid() ||
		Edit.Cut <= Root->Identity.Cut || !Edit.ResourceLease || !Edit.ResourceLease->IsOwnedBy(*Resources) ||
		Edit.Overlays.IsEmpty() || Edit.Overlays.Num() > 32768 || Edit.Heads.IsEmpty() || Edit.Heads.Num() > FVoxelOverlayRegionCodec::MaximumRegions)
	{
		Error = TEXT("InvalidResource: Delta factory requires original basis and complete captured edit");
		return EVoxelTerrainResourceAdmission::InvalidRequest;
	}
	struct FInput
	{
		const FVoxelTerrainResources& Resources;
		const FVoxelOverlayAuthorityDirectoryRoot& Root;
		const FVoxelOverlayAuthorityEditCapture& Edit;
		const FVoxelOverlayAuthorityDeltaBuild* Previous;
	} Input{*Resources, Root.Get(), Edit, OutBuild.Get()};
	FVoxelGenerationRequiredSourceScope Sources(&Resources.Get(), &Input,
		[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
		{
			const auto& Input = *static_cast<const FInput*>(Context);
			VisitRootSources(Input.Root, Input.Resources, Visit);
			VisitEditSources(Input.Edit, Input.Resources, Visit);
			if (Input.Previous && Input.Previous->NativeLease->IsOwnedBy(Input.Resources)) Input.Previous->State->VisitSources(Visit);
		});
	FVoxelGenerationRequiredCapacity Required;
	if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, Error)) return EVoxelTerrainResourceAdmission::InvalidRequest;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, NativeInputBytes(), Lease, 0,
		Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		Error = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: Delta native factory") :
			Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: Delta native factory") :
			TEXT("InvalidResource: Delta native factory domain");
		return Admission;
	}
	auto Candidate = VoxelBoundedAllocation::MakeWorkerShared<FVoxelOverlayAuthorityDeltaBuild>(Lease,
		FConstructionKey{}, Lease, Root, MoveTemp(Edit), Resources, Consumer);
	OutBuild = MoveTemp(Candidate);
	Error.Reset();
	return EVoxelTerrainResourceAdmission::Granted;
}

FVoxelOverlayAuthorityDeltaBuild::FVoxelOverlayAuthorityDeltaBuild(FConstructionKey,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InNativeLease,
	TSharedRef<const FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> Root,
	FVoxelOverlayAuthorityEditCapture&& Edit, TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources,
	const FVoxelGenerationConsumerKey& Consumer)
	: NativeLease(InNativeLease), State(MakeUnique<FState>(MoveTemp(InNativeLease), Root, MoveTemp(Edit), Resources, Consumer))
{
}

FVoxelOverlayAuthorityDeltaBuild::~FVoxelOverlayAuthorityDeltaBuild() = default;

FVoxelTaskResult FVoxelOverlayAuthorityDeltaBuild::Advance(const TAtomic<bool>& Cancel)
{
	if (Cancel.Load()) { FVoxelTaskResult Result; Result.bCanceled = true; return Result; }
	if (State->bCapacityBlocked.Load() && State->Resources->ReleaseVersionExcludingOwner(State->Consumer) <= State->CapacityVersion.Load())
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

FVoxelGenerationConsumerKey FVoxelOverlayAuthorityDeltaBuild::GetConsumer() const
{
	return State->Consumer;
}

void FVoxelOverlayDeltaEncoder::VisitDeltaSources(const FVoxelOverlayAuthorityDelta& InDelta,
	const FVoxelTerrainResources& InResources, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
{
	if (InDelta.ResourceLease && InDelta.ResourceLease->IsOwnedBy(InResources)) FVoxelGenerationRequiredSourceScope::VisitLease(InDelta.ResourceLease.Get(), InVisit);
	if (InDelta.Payload && InDelta.Payload->ResourceLease && InDelta.Payload->ResourceLease->IsOwnedBy(InResources)) FVoxelGenerationRequiredSourceScope::VisitLease(InDelta.Payload->ResourceLease.Get(), InVisit);
	if (InDelta.Root) FVoxelOverlayAuthorityDeltaBuild::VisitRootSources(*InDelta.Root, InResources, InVisit);
}

void FVoxelOverlayDeltaEncoder::VisitResourceSources(const FVoxelTerrainResources& InResources,
	FVoxelGenerationRequiredSourceScope::FVisit InVisit) const
{
	VisitDeltaSources(*Delta, InResources, InVisit);
	if (NativeLease && NativeLease->IsOwnedBy(InResources)) FVoxelGenerationRequiredSourceScope::VisitLease(NativeLease.Get(), InVisit);
}

EVoxelTerrainResourceAdmission FVoxelOverlayDeltaEncoder::Create(
	TSharedRef<const FVoxelOverlayAuthorityDelta, ESPMode::ThreadSafe> InDelta,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, const FVoxelGenerationConsumerKey& InConsumer,
	TSharedPtr<FVoxelOverlayDeltaEncoder, ESPMode::ThreadSafe>& OutEncoder, FString& OutError)
{
	if (!InDelta->Root || !InDelta->Payload || InDelta->Heads.IsEmpty() || InDelta->Heads.Num() > FVoxelOverlayRegionCodec::MaximumRegions ||
		!InConsumer.IsValid() || InConsumer.WorldEpoch != InDelta->Root->Identity.WorldEpoch)
	{
		OutError = TEXT("InvalidResource: Delta encoder original input");
		return EVoxelTerrainResourceAdmission::InvalidRequest;
	}
	struct FInput { const FVoxelTerrainResources& Resources; const FVoxelOverlayAuthorityDelta& Delta; const FVoxelOverlayDeltaEncoder* Previous; }
		Input{*InResources, *InDelta, OutEncoder.Get()};
	FVoxelGenerationRequiredSourceScope Sources(&InResources.Get(), &Input,
		[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
		{
			const auto& Input = *static_cast<const FInput*>(Context);
			VisitDeltaSources(Input.Delta, Input.Resources, Visit);
			if (Input.Previous) Input.Previous->VisitResourceSources(Input.Resources, Visit);
		});
	FVoxelGenerationRequiredCapacity Required;
	if (!FVoxelGenerationRequiredSourceScope::Collect(*InResources, nullptr, Required, OutError)) return EVoxelTerrainResourceAdmission::InvalidRequest;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const auto Admission = InResources->TryAcquire(InConsumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelOverlayDeltaEncoder>(), Lease, 0, Required.Bytes[0], Required.LeaseCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: Delta encoder native factory") :
			Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: Delta encoder native factory") :
			TEXT("InvalidResource: Delta encoder native factory");
		return Admission;
	}
	OutEncoder = VoxelBoundedAllocation::MakeWorkerShared<FVoxelOverlayDeltaEncoder>(Lease, FConstructionKey{}, Lease, InDelta, InResources, InConsumer);
	Lease->MarkPublished();
	OutError.Reset();
	return EVoxelTerrainResourceAdmission::Granted;
}

FVoxelOverlayDeltaEncoder::FVoxelOverlayDeltaEncoder(FConstructionKey, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InNativeLease,
	TSharedRef<const FVoxelOverlayAuthorityDelta, ESPMode::ThreadSafe> InDelta,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, const FVoxelGenerationConsumerKey& InConsumer)
	: NativeLease(MoveTemp(InNativeLease)), Delta(InDelta), Resources(InResources), Consumer(InConsumer)
{
}

bool FVoxelOverlayDeltaEncoder::Advance(FVoxelOverlayEncodedMessage& OutMessage, bool& Complete, FString& Error)
{
	Complete = false;
	FVoxelGenerationRequiredSourceScope Sources(&Resources.Get(), this,
		[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
		{
			const auto& Encoder = *static_cast<const FVoxelOverlayDeltaEncoder*>(Context);
			Encoder.VisitResourceSources(*Encoder.Resources, Visit);
		});
	FVoxelGenerationRequiredSourceScope PreviousOutput(&Resources.Get(), OutMessage.ResourceLease.Get());
	if (!Delta->Root || !Delta->Payload || Delta->Heads.IsEmpty()) { Error = TEXT("增量编码缺少完整事务"); return false; }
	const uint32 Count = FMath::DivideAndRoundUp(Delta->Payload->Bytes.Num(), FVoxelOverlayRegionCodec::MaximumPartBytes);
	if (bHeaderSent && Part == Count) { Complete = true; Error.Reset(); return true; }
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Output;
	const int32 HeadCount = bHeaderSent ? 0 : Delta->Heads.Num();
	const int32 PayloadCount = bHeaderSent ? FMath::Min<int32>(FVoxelOverlayRegionCodec::MaximumPartBytes,
		Delta->Payload->Bytes.Num() - Part * FVoxelOverlayRegionCodec::MaximumPartBytes) : 0;
	if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Working,
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelOverlayRegionHead, FDefaultAllocator>(HeadCount) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(PayloadCount), Scratch, Error)) return false;
	FVoxelGenerationRequiredSourceScope ScratchSource(&Resources.Get(), Scratch.Get());
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
	int32 EncodedBytes = 0;
	if (!FVoxelOverlayRegionCodec::EncodedByteCount(Message, EncodedBytes)) { Error = TEXT("InvalidResource: Delta message encoded count"); return false; }
	if (!Grant(Resources, Consumer, EVoxelTerrainResourcePool::Staged,
		VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(EncodedBytes), Output, Error)) return false;
	TArray<uint8> Bytes;
	if (!FVoxelOverlayRegionCodec::Encode(EVoxelMessage::RegionDelta, Message, Bytes)) { Error = TEXT("增量消息编码失败"); return false; }
	if (Bytes.Num() != EncodedBytes || VoxelGenerationAllocation::AllocatedBytes(Bytes) > Output->Bytes())
	{
		Error = TEXT("EstimateMismatch: 增量消息输出容量");
		return false;
	}
	Output->Resize(VoxelGenerationAllocation::AllocatedBytes(Bytes));
	OutMessage.Kind = EVoxelMessage::RegionDelta;
	OutMessage.Bytes = MoveTemp(Bytes);
	OutMessage.ResourceLease = MoveTemp(Output);
	OutMessage.ResourceLease->MarkPublished();
	if (bHeaderSent) ++Part;
	else bHeaderSent = true;
	Error.Reset();
	return true;
}
