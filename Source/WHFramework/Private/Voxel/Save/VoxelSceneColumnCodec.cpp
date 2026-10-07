#include "Voxel/Save/VoxelSceneColumnCodec.h"

#include "SaveGame/SaveDataSerializer.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Save/VoxelSceneColumnCapture.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

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
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SortLease;
	const uint64 SortBytes = VoxelBoundedAllocation::ArrayBytes<int32>(InCapture.Actors.Num());
	const uint64 OutputBytes = VoxelBoundedAllocation::ArrayBytes<uint8>(int32(ByteCount));
	if ((!InCapture.Actors.IsEmpty() && !InCapture.Resources->Acquire(EncodingConsumer, EVoxelTerrainResourcePool::Working,
		SortBytes, SortLease)) ||
		!InCapture.Resources->Acquire(EncodingConsumer, EVoxelTerrainResourcePool::Input,
			OutputBytes, Completion.ResourceLease))
	{
		OutError = TEXT("CapacityBlocked: Scene column sort/output backing was not granted");
		return false;
	}
	TArray<int32> Order;
	Order.Reserve(InCapture.Actors.Num());
	check(Order.GetAllocatedSize() <= SortBytes);
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
	check(Completion.Bytes.GetAllocatedSize() <= OutputBytes);
	verify(Completion.ResourceLease->Resize(Completion.Bytes.GetAllocatedSize()));
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
