#include "Voxel/Runtime/VoxelFineOverlayRead.h"

#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

uint64 FVoxelFineOverlayCapture::AllocatedBytes() const
{
	uint64 Bytes = sizeof(*this) + Resident.GetAllocatedBytes() + Reads.GetAllocatedSize() + ContentRevisions.GetAllocatedSize() + ReplicaRegionSequences.GetAllocatedSize() +
		(ReadLeaseSet ? ReadLeaseSet->GetMetadataBytes() : 0);
	for (const FVoxelRegionReadView& Read : Reads)
	{
		Bytes += Read.SourceDirectory.GetAllocatedSize();
	}
	return Bytes;
}

uint64 FVoxelFineOverlayReadResult::AllocatedBytes() const
{
	return sizeof(*this) + ContentRevisions.GetAllocatedSize() + ReplicaRegionSequences.GetAllocatedSize() +
		(Snapshot ? Snapshot->GetAllocatedBytes() : 0) +
		(ReadLeaseSet ? ReadLeaseSet->GetMetadataBytes() : 0);
}

bool FVoxelFineOverlayRead::Initialize(FVoxelFineOverlayCapture&& InCapture, FString& OutError)
{
	if (State != EVoxelFineOverlayReadState::None || InCapture.WorldEpoch == 0 || !InCapture.Registry ||
		!InCapture.Bounds.IsValid() || !InCapture.ReadLeaseSet || InCapture.ReadLeaseSet->GetWorldEpoch() != InCapture.WorldEpoch)
	{
		OutError = TEXT("Overlay 解码必须接收同一不可变目录切点下的完整读取租约");
		return false;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ValidationLease;
	if (InCapture.Resources && !InCapture.Reads.IsEmpty() && !InCapture.Resources->Acquire(InCapture.Consumer, EVoxelTerrainResourcePool::Working,
		VoxelBoundedAllocation::SetBytes<FIntVector>(InCapture.Reads.Num()), ValidationLease))
	{
		OutError = TEXT("CapacityBlocked: Overlay 读取描述校验容量不足");
		return false;
	}
	if (!InCapture.ReadLeaseSet->ValidateCapture(InCapture, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("ReadView、驻留 Overlay 或文件代不属于所提供的读取租约");
		}
		return false;
	}
	Capture = MoveTemp(InCapture);
	Snapshot = MakeShared<FVoxelOverlaySnapshotSet, ESPMode::ThreadSafe>(MoveTemp(Capture.Resident));
	Snapshot->Sections.Reserve(Capture.Reads.Num() + Snapshot->Sections.Num());
	State = Capture.Reads.IsEmpty() ? EVoxelFineOverlayReadState::Complete : EVoxelFineOverlayReadState::Working;
	OutError.Reset();
	return true;
}

bool FVoxelFineOverlayRead::Fail(const EVoxelFineOverlayReadState InState, FString InError, FString& OutError)
{
	check(InState == EVoxelFineOverlayReadState::Canceled || InState == EVoxelFineOverlayReadState::Failed);
	State = InState;
	Failure = MoveTemp(InError);
	OutError = Failure;
	return false;
}

bool FVoxelFineOverlayRead::Advance(const int32 InMaximumReads, const TAtomic<bool>& InCancel,
	bool& bOutComplete, FString& OutError)
{
	bOutComplete = false;
	if (State == EVoxelFineOverlayReadState::Canceled || State == EVoxelFineOverlayReadState::Failed)
	{
		OutError = Failure;
		return false;
	}
	if (State != EVoxelFineOverlayReadState::Working && State != EVoxelFineOverlayReadState::Complete)
	{
		OutError = TEXT("Overlay 读取不处于可推进状态");
		return false;
	}
	if (InCancel.Load())
	{
		return Fail(EVoxelFineOverlayReadState::Canceled, TEXT("Canceled"), OutError);
	}
	if (State == EVoxelFineOverlayReadState::Complete)
	{
		bOutComplete = true;
		OutError.Reset();
		return true;
	}
	if (!Snapshot || !Capture.ReadLeaseSet || InMaximumReads <= 0)
	{
		return Fail(EVoxelFineOverlayReadState::Failed, TEXT("Overlay 解码任务的快照、读取租约或批次限额无效"), OutError);
	}
	for (int32 Work = 0; Work < InMaximumReads && Capture.Reads.IsValidIndex(NextRead); ++Work)
	{
		if (InCancel.Load())
		{
			return Fail(EVoxelFineOverlayReadState::Canceled, TEXT("Canceled"), OutError);
		}
		const FVoxelRegionReadView& Read = Capture.Reads[NextRead];
		FString ReadError;
		if (PendingBytes.IsEmpty())
		{
			if (Capture.Resources && !Capture.Resources->Acquire(Capture.Consumer, EVoxelTerrainResourcePool::Working,
				FVoxelRegionStore::ReadWorkingBytes(Read), PendingReadLease))
			{
				OutError = TEXT("CapacityBlocked: 固定文件代读取峰值未准入");
				return true;
			}
			if (FVoxelRegionStore::Read(Read, PendingBytes, ReadError) != EVoxelRegionRead::Loaded)
			{
				return Fail(EVoxelFineOverlayReadState::Failed,
					ReadError.IsEmpty() ? TEXT("已修改 Section 的固定文件代增量缺失") : MoveTemp(ReadError), OutError);
			}
		}
		if (InCancel.Load())
		{
			return Fail(EVoxelFineOverlayReadState::Canceled, TEXT("Canceled"), OutError);
		}
		TSharedPtr<FVoxelPersistentSection, ESPMode::ThreadSafe> Decoded;
		if (Capture.Resources)
		{
			if (!Decoder.Advance(PendingBytes, Capture.Manifest, *Capture.Registry, Capture.Resources.ToSharedRef(),
				Capture.Consumer, EVoxelTerrainResourcePool::Input, Decoded, ReadError))
			{
				if (ReadError.StartsWith(TEXT("CapacityBlocked:"))) { OutError = MoveTemp(ReadError); return true; }
				return Fail(EVoxelFineOverlayReadState::Failed, MoveTemp(ReadError), OutError);
			}
		}
		else
		{
			Decoded = MakeShared<FVoxelPersistentSection, ESPMode::ThreadSafe>();
			if (!FVoxelDeltaCodec::Decode(PendingBytes, Capture.Manifest, *Capture.Registry, *Decoded))
				return Fail(EVoxelFineOverlayReadState::Failed, TEXT("持久化增量解码失败"), OutError);
		}
		auto& Persistent = *Decoded;
		if (Persistent.Section != Read.Section)
		{
			return Fail(EVoxelFineOverlayReadState::Failed, TEXT("持久化增量解码或 Section 身份校验失败"), OutError);
		}
		if (!Capture.ReadLeaseSet->ValidateRead(Read, PendingBytes, Persistent.Revision, ReadError))
		{
			return Fail(EVoxelFineOverlayReadState::Failed,
				ReadError.IsEmpty() ? TEXT("读取内容与固定代记录的版本或完整性 Hash 不符") : MoveTemp(ReadError), OutError);
		}
		FVoxelOverlaySnapshot Entry;
		Entry.Section = Persistent.Section;
		Entry.Revision = Persistent.Revision;
		Entry.Blocks = MoveTemp(Persistent.Blocks);
		Entry.Entities = MoveTemp(Persistent.Entities);
		Entry.ResourceLease = MoveTemp(Persistent.ResourceLease);
		Decoded.Reset();
		if (Entry.ResourceLease)
		{
			uint64 Actual = Entry.Blocks.GetAllocatedSize() + Entry.Entities.GetAllocatedSize();
			for (const auto& Entity : Entry.Entities) Actual += Entity.Value.Payload.GetAllocatedSize();
			if (Actual > Entry.ResourceLease->Bytes()) return Fail(EVoxelFineOverlayReadState::Failed,
				TEXT("EstimateMismatch: 固定文件代解码输出容量"), OutError);
			// 退休头与删除任务仍由同一租约持有，直到这个不可变条目退休后再统一释放。
		}
		Snapshot->Sections.Add(Entry.Section, MoveTemp(Entry));
		PendingBytes.Empty();
		PendingReadLease.Reset();
		++NextRead;
	}
	if (NextRead == Capture.Reads.Num())
	{
		State = EVoxelFineOverlayReadState::Complete;
	}
	bOutComplete = State == EVoxelFineOverlayReadState::Complete;
	OutError.Reset();
	return true;
}

bool FVoxelFineOverlayRead::TakeResult(FVoxelFineOverlayReadResult& OutResult)
{
	if (State != EVoxelFineOverlayReadState::Complete || !Snapshot || !Capture.ReadLeaseSet)
	{
		return false;
	}
	FVoxelFineOverlayReadResult Result;
	Result.WorldEpoch = Capture.WorldEpoch;
	Result.Bounds = Capture.Bounds;
	Result.ContentRevisions = MoveTemp(Capture.ContentRevisions);
	Result.ReplicaDirectoryEpoch = Capture.ReplicaDirectoryEpoch;
	Result.ReplicaRegionSequences = MoveTemp(Capture.ReplicaRegionSequences);
	Result.VersionProof = MoveTemp(Capture.VersionProof);
	Result.Snapshot = MoveTemp(Snapshot);
	Result.ReadLeaseSet = MoveTemp(Capture.ReadLeaseSet);
	Result.Resources = MoveTemp(Capture.Resources);
	Result.Consumer = Capture.Consumer;
	if (Result.Snapshot->ResourceLease) Result.ResourceAccountedBytes += Result.Snapshot->ResourceLease->Bytes();
	if (Result.VersionProof && Result.VersionProof->ResourceLease)
		Result.ResourceAccountedBytes += Result.VersionProof->ResourceLease->Bytes();
	Result.Snapshot->Visit([&Result](const FIntVector&, const FVoxelOverlaySnapshot& Entry)
	{
		if (Entry.ResourceLease) Result.ResourceAccountedBytes += Entry.ResourceLease->Bytes();
		if (Entry.IndexLease && Entry.IndexLease != Entry.ResourceLease) Result.ResourceAccountedBytes += Entry.IndexLease->Bytes();
	});
	OutResult = MoveTemp(Result);
	Capture.Reads.Reset();
	Capture.Registry.Reset();
	State = EVoxelFineOverlayReadState::Taken;
	return true;
}

uint64 FVoxelFineOverlayRead::GetWorldEpoch() const
{
	return Capture.WorldEpoch;
}

uint64 FVoxelFineOverlayRead::AllocatedBytes() const
{
	return sizeof(*this) + Capture.AllocatedBytes() - sizeof(Capture) + Failure.GetAllocatedSize() +
		PendingBytes.GetAllocatedSize() + Decoder.GetRetainedWorkingBytes() + (Snapshot ? Snapshot->GetAllocatedBytes() : 0);
}

EVoxelFineOverlayReadState FVoxelFineOverlayRead::GetState() const
{
	return State;
}
