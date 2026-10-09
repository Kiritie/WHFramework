#include "Voxel/Save/VoxelSceneColumnCodec.h"

#include "SaveGame/SaveDataSerializer.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Save/VoxelSceneColumnCapture.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

FVoxelSceneColumnCaptureCursor& FVoxelSceneColumnCaptureCursor::operator=(FVoxelSceneColumnCaptureCursor&& InOther)
{
	if (this == &InOther) return *this;
	// 旧反射正文先销毁；旧 lease 与来方 lease 都覆盖到数据移动结束。
	ProjectData = MoveTemp(InOther.ProjectData);
	ProjectStateLease = MoveTemp(InOther.ProjectStateLease);
	Capture = MoveTemp(InOther.Capture);
	Consumer = InOther.Consumer;
	RegionInstanceId = InOther.RegionInstanceId;
	WorldEpoch = InOther.WorldEpoch;
	Revision = InOther.Revision;
	ProjectRevision = InOther.ProjectRevision;
	BlockedReleaseVersion = InOther.BlockedReleaseVersion;
	Phase = InOther.Phase;
	PendingCursor = InOther.PendingCursor;
	ActorCursor = InOther.ActorCursor;
	ProjectCursor = InOther.ProjectCursor;
	ActorCapacity = InOther.ActorCapacity;
	bCapacityBlocked = InOther.bCapacityBlocked;
	return *this;
}

void FVoxelSceneColumnCapture::VisitRequiredSources(const FVoxelTerrainResources& InResources,
	FVoxelGenerationRequiredSourceScope::FVisit InVisit) const
{
	const auto Visit = [&](const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
	{
		if (Lease && Lease->IsOwnedBy(InResources)) FVoxelGenerationRequiredSourceScope::VisitLease(Lease.Get(), InVisit);
	};
	Visit(MetadataLease);
	Visit(ProjectLease);
	if (Completion) Visit(Completion->ResourceLease);
	for (const auto& Actor : Actors) Visit(Actor.ResourceLease);
}

EVoxelTerrainResourceAdmission FVoxelSceneColumnCapture::TryGrantCapacity(FVoxelTerrainResources& InResources,
	const FVoxelGenerationConsumerKey& InConsumer, const EVoxelTerrainResourcePool InPool, const uint64 InBytes,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InOutLease, FString& OutError)
{
	EVoxelTerrainResourceAdmission Admission = EVoxelTerrainResourceAdmission::InvalidRequest;
	if (InConsumer.IsValid() && InPool < EVoxelTerrainResourcePool::Count &&
		(!InOutLease || (InOutLease->IsOwnedBy(InResources) && InOutLease->GetPool() == InPool)))
	{
		FVoxelGenerationRequiredCapacity Required;
		if (FVoxelGenerationRequiredSourceScope::Collect(InResources, InOutLease.Get(), Required, OutError))
		{
			const auto Limits = InResources.Snapshot().Limits;
			const uint64 PoolLimits[] = {Limits.InputBytes, Limits.WorkingBytes, Limits.StagedBytes,
				Limits.ResidentBytes - Limits.OverlapBytes, Limits.OverlapBytes};
			const uint64 RequiredBytes = Required.Bytes[int32(InPool)];
			if (RequiredBytes > PoolLimits[int32(InPool)] || InBytes > PoolLimits[int32(InPool)] - RequiredBytes)
				Admission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
			else if (InOutLease)
				Admission = InOutLease->TryResize(InBytes, Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
			else if (InBytes == 0)
				Admission = EVoxelTerrainResourceAdmission::Granted;
			else
				Admission = InResources.TryAcquire(InConsumer, InPool, InBytes, InOutLease, 0,
					Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
		}
		else Admission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
	}
	if (Admission == EVoxelTerrainResourceAdmission::Granted) OutError.Reset();
	else OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ?
		TEXT("CapacityBlocked: original Scene column backing was not granted") :
		Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
		TEXT("CapacityUnachievable: original Scene column backing cannot fit its resource domain") :
		TEXT("InvalidRequest: original Scene column resource owner");
	return Admission;
}

FString FVoxelSceneColumnCodec::RelativePath(const FIntPoint InColumn)
{
	return FString::Printf(TEXT("voxel/actors/c_%d_%d.bin"), InColumn.X, InColumn.Y);
}

bool FVoxelSceneColumnCodec::Encode(
	const TArray<FVoxelSavedSceneActor>& InActors,
	const FParameter& InProjectData,
	TArray<uint8>& OutBytes,
	FString& OutError)
{
	check(IsInGameThread());
	if (InActors.Num() > 1024)
	{
		OutError = TEXT("Too many scene actors in one column");
		return false;
	}

	TArray<FVoxelSavedSceneActor> SortedActors = InActors;
	SortedActors.Sort([](const FVoxelSavedSceneActor& InA, const FVoxelSavedSceneActor& InB)
	{
		return InA.Id.ToString() < InB.Id.ToString();
	});

	FVoxelByteWriter Writer(8 * 1024 * 1024);
	Writer.U32(0x33414356);
	Writer.U16(static_cast<uint16>(SortedActors.Num()));
	TSet<FGuid> SeenIds;
	for (FVoxelSavedSceneActor& Actor : SortedActors)
	{
		TArray<uint8> Payload;
		if (!Actor.Id.IsValid() ||
			SeenIds.Contains(Actor.Id) ||
			!Actor.Data.HasValue() ||
			!FSaveDataSerializer::MarkSaveDataSaved(Actor.Data) ||
			!FSaveDataSerializer::SerializeParameter(Actor.Data, Payload) ||
			Payload.Num() > 256 * 1024)
		{
			OutError = TEXT("Invalid scene actor save payload");
			return false;
		}
		SeenIds.Add(Actor.Id);
		Writer.Guid(Actor.Id);
		Writer.Blob(Payload, 256 * 1024);
	}

	TArray<uint8> ProjectPayload;
	if (InProjectData.HasValue())
	{
		FParameter ProjectCopy = InProjectData;
		if (!FSaveDataSerializer::MarkSaveDataSaved(ProjectCopy) ||
			!FSaveDataSerializer::SerializeParameter(ProjectCopy, ProjectPayload) ||
			ProjectPayload.Num() > 1024 * 1024)
		{
			OutError = TEXT("Invalid scene project metadata");
			return false;
		}
	}
	Writer.Blob(ProjectPayload, 1024 * 1024);
	if (!Writer.Finish(OutBytes))
	{
		OutError = TEXT("Scene column save exceeds 8MiB");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FVoxelSceneColumnCodec::EncodePayloads(const FVoxelSceneColumnCapture& InCapture, FString& OutError,
	const FVoxelGenerationConsumerKey* InEncodingConsumer)
{
	if (!InCapture.Resources || !InCapture.Completion || InCapture.Actors.Num() > 1024 || InCapture.ProjectBytes.Num() > 1024 * 1024)
	{
		OutError = TEXT("Invalid frozen Scene column payload capture");
		return false;
	}
	auto& Completion = *InCapture.Completion;
	if (Completion.bEncoded) return true;
	const auto& EncodingConsumer = InEncodingConsumer ? *InEncodingConsumer : InCapture.Consumer;
	if (!EncodingConsumer.IsValid() || EncodingConsumer.WorldEpoch != InCapture.Consumer.WorldEpoch)
	{
		OutError = TEXT("Scene column encoding owner does not match its frozen world");
		return false;
	}
	uint64 ByteCount = 6 + 4 + InCapture.ProjectBytes.Num();
	for (const auto& Actor : InCapture.Actors)
	{
		if (!Actor.Id.IsValid() || Actor.Bytes.IsEmpty() || Actor.Bytes.Num() > 256 * 1024)
		{
			OutError = TEXT("Invalid frozen Scene actor payload");
			return false;
		}
		ByteCount += 16 + 4 + Actor.Bytes.Num();
	}
	if (ByteCount > 8 * 1024 * 1024)
	{
		OutError = TEXT("Scene column save exceeds 8MiB");
		return false;
	}
	FVoxelGenerationRequiredSourceScope CaptureSource(InCapture.Resources.Get(), &InCapture,
		[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
		{
			const auto& Capture = *static_cast<const FVoxelSceneColumnCapture*>(Context);
			Capture.VisitRequiredSources(*Capture.Resources, Visit);
		});
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SortLease;
	const uint64 SortBytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<int32, FDefaultAllocator>(InCapture.Actors.Num());
	const uint64 OutputBytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(int32(ByteCount));
	if (FVoxelSceneColumnCapture::TryGrantCapacity(*InCapture.Resources, EncodingConsumer,
		EVoxelTerrainResourcePool::Working, SortBytes, SortLease, OutError) != EVoxelTerrainResourceAdmission::Granted) return false;
	FVoxelGenerationRequiredSourceScope SortSource(InCapture.Resources.Get(), SortLease.Get());
	const uint64 PreviousOutput = VoxelGenerationAllocation::AllocatedBytes(Completion.Bytes);
	if (OutputBytes > MAX_uint64 - PreviousOutput ||
		FVoxelSceneColumnCapture::TryGrantCapacity(*InCapture.Resources, EncodingConsumer,
			EVoxelTerrainResourcePool::Input, PreviousOutput + OutputBytes, Completion.ResourceLease, OutError) !=
			EVoxelTerrainResourceAdmission::Granted) return false;
	TArray<int32> Order;
	Order.Reserve(InCapture.Actors.Num());
	check(VoxelGenerationAllocation::AllocatedBytes(Order) <= SortBytes);
	for (int32 Index = 0; Index < InCapture.Actors.Num(); ++Index) Order.Add(Index);
	Order.Sort([&](const int32 Left, const int32 Right)
	{
		const auto& A = InCapture.Actors[Left].Id;
		const auto& B = InCapture.Actors[Right].Id;
		return A.A != B.A ? A.A < B.A : A.B != B.B ? A.B < B.B : A.C != B.C ? A.C < B.C : A.D < B.D;
	});
	FVoxelByteWriter Writer(8 * 1024 * 1024);
	if (!Writer.Reserve(int32(ByteCount))) return false;
	Writer.U32(0x33414356);
	Writer.U16(uint16(Order.Num()));
	FGuid Previous;
	for (const int32 Index : Order)
	{
		const auto& Actor = InCapture.Actors[Index];
		if (Actor.Id == Previous)
		{
			OutError = TEXT("Duplicate frozen Scene actor payload identity");
			return false;
		}
		Previous = Actor.Id;
		Writer.Guid(Actor.Id);
		Writer.Blob(Actor.Bytes, 256 * 1024);
	}
	Writer.Blob(InCapture.ProjectBytes, 1024 * 1024);
	if (!Writer.Finish(Completion.Bytes)) return false;
	check(VoxelGenerationAllocation::AllocatedBytes(Completion.Bytes) <= OutputBytes);
	verify(Completion.ResourceLease->TryResize(VoxelGenerationAllocation::AllocatedBytes(Completion.Bytes)) ==
		EVoxelTerrainResourceAdmission::Granted);
	Completion.bEncoded = true;
	OutError.Reset();
	return true;
}

bool FVoxelSceneColumnCodec::Decode(
	const TConstArrayView<uint8> InBytes,
	TArray<FVoxelSavedSceneActor>& OutActors,
	FParameter& OutProjectData,
	FString& OutError)
{
	check(IsInGameThread());
	if (InBytes.Num() > 8 * 1024 * 1024)
	{
		OutError = TEXT("Scene column save exceeds 8MiB");
		return false;
	}

	FVoxelByteReader Reader(InBytes);
	if (Reader.U32() != 0x33414356)
	{
		OutError = TEXT("Invalid scene column header");
		return false;
	}
	const uint16 ActorCount = Reader.U16();
	if (ActorCount > 1024)
	{
		OutError = TEXT("Too many scene actors in one column");
		return false;
	}

	TArray<FVoxelSavedSceneActor> Actors;
	TSet<FGuid> SeenIds;
	for (uint16 Index = 0; Index < ActorCount; ++Index)
	{
		FVoxelSavedSceneActor Actor;
		Actor.Id = Reader.Guid();
		const TArray<uint8> Payload = Reader.Blob(256 * 1024);
		if (!Reader.IsValid())
		{
			OutError = TEXT("Invalid scene actor record framing");
			return false;
		}
		if (!Actor.Id.IsValid() || SeenIds.Contains(Actor.Id))
		{
			OutError = TEXT("Invalid or duplicate scene actor id");
			return false;
		}
		if (!FSaveDataSerializer::DeserializeParameter(Payload, Actor.Data))
		{
			OutError = TEXT("Invalid scene actor parameter payload");
			return false;
		}
		SeenIds.Add(Actor.Id);
		Actors.Add(MoveTemp(Actor));
	}

	const TArray<uint8> ProjectPayload = Reader.Blob(1024 * 1024);
	FParameter ProjectData;
	if (!ProjectPayload.IsEmpty() &&
		!FSaveDataSerializer::DeserializeParameter(ProjectPayload, ProjectData))
	{
		OutError = TEXT("Invalid scene project metadata");
		return false;
	}
	if (!Reader.End())
	{
		OutError = TEXT("Scene column contains trailing or truncated data");
		return false;
	}

	OutActors = MoveTemp(Actors);
	OutProjectData = MoveTemp(ProjectData);
	OutError.Reset();
	return true;
}
