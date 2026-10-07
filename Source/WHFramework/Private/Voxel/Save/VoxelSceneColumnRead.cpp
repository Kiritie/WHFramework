#include "Voxel/Save/VoxelSceneColumnRead.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Voxel/Save/VoxelSceneColumnCodec.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

namespace
{
	bool ReadSceneColumnFraming(const TConstArrayView<uint8> InBytes, FString& OutError,
		FVoxelSceneColumnRead* OutRead = nullptr)
	{
		if (InBytes.Num() < 10 || InBytes.Num() > 8 * 1024 * 1024)
		{
			OutError = TEXT("Scene column size is outside its protocol limit");
			return false;
		}
		FVoxelByteReader Reader(InBytes);
		if (Reader.U32() != 0x33414356)
		{
			OutError = TEXT("Invalid scene column header");
			return false;
		}
		const uint16 Count = Reader.U16();
		if (!Reader.IsValid() || Count > 1024)
		{
			OutError = TEXT("Invalid scene column actor count");
			return false;
		}
		FGuid Seen[1024];
		for (uint16 Index = 0; Index < Count; ++Index)
		{
			const FGuid Id = Reader.Guid();
			const auto Payload = Reader.BlobView(256 * 1024);
			if (!Reader.IsValid() || !Id.IsValid() || Payload.IsEmpty())
			{
				OutError = TEXT("Invalid scene actor payload framing");
				return false;
			}
			for (uint16 Previous = 0; Previous < Index; ++Previous)
			{
				if (Seen[Previous] == Id)
				{
					OutError = TEXT("Duplicate stored scene actor GUID");
					return false;
				}
			}
			Seen[Index] = Id;
			if (OutRead) OutRead->Actors.Add({Id, int32(Payload.GetData() - InBytes.GetData()), Payload.Num()});
		}
		const auto Project = Reader.BlobView(1024 * 1024);
		if (!Reader.End())
		{
			OutError = TEXT("Scene column contains trailing or truncated data");
			return false;
		}
		if (OutRead)
		{
			OutRead->ProjectOffset = Project.IsEmpty() ? 0 : int32(Project.GetData() - InBytes.GetData());
			OutRead->ProjectBytes = Project.Num();
		}
		OutError.Reset();
		return true;
	}
}

bool FVoxelSceneColumnRead::Validate(const TConstArrayView<uint8> InBytes, FString& OutError)
{
	return ReadSceneColumnFraming(InBytes, OutError);
}

bool FVoxelSceneColumnRead::Load(
	const TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe>& InSource,
	const FIntPoint InColumn, const uint32 InExpectedBytes,
	const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
	const FVoxelGenerationConsumerKey& InConsumer,
	TSharedPtr<const FVoxelSceneColumnRead, ESPMode::ThreadSafe>& OutRead,
	FString& OutError, const TAtomic<bool>* InCancel, const TAtomic<bool>* InDetached)
{
	OutError.Reset();
	if (!InSource || !InResources || !InConsumer.IsValid() || OutRead ||
		!InSource->IsResourceAccountedFor(InResources.Get(), InConsumer.WorldEpoch) ||
		InExpectedBytes < 10 || InExpectedBytes > 8 * 1024 * 1024)
	{
		OutError = TEXT("Scene column read has no valid pinned source/size/resource owner");
		return false;
	}
	const auto Canceled = [&]()
	{
		return (InCancel && InCancel->Load()) || (InDetached && InDetached->Load());
	};
	if (Canceled())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PathLease;
	const uint64 PathBytes = 2 * VoxelBoundedAllocation::ArrayBytes<TCHAR>(InSource->Directory().Len() + 96);
	if (!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Working, PathBytes, PathLease))
	{
		OutError = TEXT("CapacityBlocked: Scene read path backing was not granted");
		return false;
	}
	const FString Path = FPaths::Combine(InSource->Directory(), FVoxelSceneColumnCodec::RelativePath(InColumn));
	TUniquePtr<IFileHandle> File(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*Path));
	if (!File || File->Size() != InExpectedBytes)
	{
		OutError = TEXT("Pinned Scene column is missing or no longer matches its directory descriptor");
		return false;
	}
	uint8 Header[6];
	if (!File->Read(Header, UE_ARRAY_COUNT(Header)))
	{
		OutError = TEXT("Scene column header read failed");
		return false;
	}
	FVoxelByteReader HeaderReader(MakeArrayView(Header));
	const uint32 Magic = HeaderReader.U32();
	const uint16 Count = HeaderReader.U16();
	if (Magic != 0x33414356 || Count > 1024 || !HeaderReader.End() || !File->Seek(0))
	{
		OutError = TEXT("Invalid Scene column header/count");
		return false;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BodyLease;
	const uint64 MetadataBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSceneColumnRead>() +
		VoxelBoundedAllocation::ArrayBytes<FVoxelSceneActorRead>(Count);
	const uint64 BodyBytes = VoxelBoundedAllocation::ArrayBytes<uint8>(int32(InExpectedBytes));
	if (!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input, MetadataBytes, MetadataLease) ||
		!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input, BodyBytes, BodyLease))
	{
		OutError = TEXT("CapacityBlocked: Scene read metadata/body backing was not granted");
		return false;
	}
	auto Read = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSceneColumnRead>(MetadataLease);
	Read->MetadataLease = MoveTemp(MetadataLease);
	Read->SourceLease = InSource;
	Read->Consumer = InConsumer;
	Read->Body.ResourceLease = MoveTemp(BodyLease);
	Read->Body.Bytes.Reserve(int32(InExpectedBytes));
	Read->Body.Bytes.SetNumUninitialized(int32(InExpectedBytes), EAllowShrinking::No);
	Read->Actors.Reserve(Count);
	check(Read->Body.Bytes.GetAllocatedSize() <= BodyBytes);
	check(Read->Actors.GetAllocatedSize() <= VoxelBoundedAllocation::ArrayBytes<FVoxelSceneActorRead>(Count));
	for (int32 Offset = 0; Offset < Read->Body.Bytes.Num();)
	{
		if (Canceled())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		const int32 Bytes = FMath::Min(64 * 1024, Read->Body.Bytes.Num() - Offset);
		if (!File->Read(Read->Body.Bytes.GetData() + Offset, Bytes))
		{
			OutError = TEXT("Scene column body read failed");
			return false;
		}
		Offset += Bytes;
	}
	FVoxelByteReader BodyHeader(Read->Body.Bytes);
	if (BodyHeader.U32() != Magic || BodyHeader.U16() != Count)
	{
		OutError = TEXT("Scene column header changed during its pinned read");
		return false;
	}
	if (!ReadSceneColumnFraming(Read->Body.Bytes, OutError, Read.Get())) return false;
	if (Canceled())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	verify(Read->Body.ResourceLease->Resize(Read->Body.Bytes.GetAllocatedSize()));
	OutRead = MoveTemp(Read);
	return true;
}

TConstArrayView<uint8> FVoxelSceneColumnRead::ActorPayload(const int32 InIndex) const
{
	const auto& Actor = Actors[InIndex];
	return MakeArrayView(Body.Bytes.GetData() + Actor.Offset, Actor.Bytes);
}

TConstArrayView<uint8> FVoxelSceneColumnRead::ProjectPayload() const
{
	return ProjectBytes > 0 ? MakeArrayView(Body.Bytes.GetData() + ProjectOffset, ProjectBytes) : TConstArrayView<uint8>();
}
