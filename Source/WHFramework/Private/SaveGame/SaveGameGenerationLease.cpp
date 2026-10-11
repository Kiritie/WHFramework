#include "SaveGame/SaveGameGenerationLease.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include <atomic>

namespace
{
	struct FSaveGenerationRegistry
	{
		FCriticalSection Mutex;
		SaveGameGeneration::FDirectoryLink* Head = nullptr;
		uint64 NextIdentity = 0;
		std::atomic_uint64_t WriterReleaseVersion{0};
	};
	FSaveGenerationRegistry& SaveGenerationRegistry()
	{
		static FSaveGenerationRegistry Value;
		return Value;
	}
	void AdvanceSaveWriterReleaseVersion(FSaveGenerationRegistry& InRegistry)
	{
		const uint64 Previous = InRegistry.WriterReleaseVersion.fetch_add(1, std::memory_order_release);
		check(Previous != MAX_uint64);
	}
	FString CanonicalSaveGenerationDirectory(const FString& InDirectory)
	{
		FString Directory = FPaths::ConvertRelativePathToFull(InDirectory);
		FPaths::NormalizeDirectoryName(Directory);
		FPaths::CollapseRelativeDirectories(Directory);
#if PLATFORM_WINDOWS
		Directory.ToLowerInline();
#endif
		return Directory;
	}
	bool ContainsSaveGenerationDirectory(const FString& InParent, const FString& InChild)
	{
		return InParent == InChild || (InChild.Len() > InParent.Len() &&
			(InParent.EndsWith(TEXT("/")) || InChild[InParent.Len()] == '/') &&
			InChild.StartsWith(InParent, ESearchCase::CaseSensitive));
	}
	void RegisterSaveDirectory(FSaveGenerationRegistry& InRegistry, SaveGameGeneration::FDirectoryLink& InLink)
	{
		check(InLink.Access != SaveGameGeneration::EDirectoryAccess::None && !InLink.Previous && !InLink.Next);
		InLink.Next = InRegistry.Head;
		if (InRegistry.Head) InRegistry.Head->Previous = &InLink;
		InRegistry.Head = &InLink;
	}
	void UnregisterSaveDirectory(FSaveGenerationRegistry& InRegistry, SaveGameGeneration::FDirectoryLink& InLink)
	{
		check(InLink.Access != SaveGameGeneration::EDirectoryAccess::None);
		if (InLink.Previous) InLink.Previous->Next = InLink.Next;
		else
		{
			check(InRegistry.Head == &InLink);
			InRegistry.Head = InLink.Next;
		}
		if (InLink.Next) InLink.Next->Previous = InLink.Previous;
		InLink.Previous = InLink.Next = nullptr;
		InLink.Access = SaveGameGeneration::EDirectoryAccess::None;
	}
	uint64 ResolveSaveDirectoryIdentity(FSaveGenerationRegistry& InRegistry, const FString& InDirectory)
	{
		for (const auto* Link = InRegistry.Head; Link; Link = Link->Next)
		{
			if (Link->Identity != 0 && Link->Directory == InDirectory) return Link->Identity;
		}
		checkf(InRegistry.NextIdentity != MAX_uint64, TEXT("Save file generation identity exhausted"));
		return ++InRegistry.NextIdentity;
	}
}

FSaveGameGenerationReadLease::FSaveGameGenerationReadLease(FString InDirectory, const uint64 InIdentity,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease,
	const FVoxelTerrainResources* InResources, const uint64 InWorldEpoch)
	: ResourceLease(MoveTemp(InLease)), ResourceOwner(InResources), ResourceWorldEpoch(InWorldEpoch)
{
	Link.Directory = MoveTemp(InDirectory);
	Link.Identity = InIdentity;
	Link.Access = SaveGameGeneration::EDirectoryAccess::Reader;
}

FSaveGameGenerationReadLease::~FSaveGameGenerationReadLease()
{
	FSaveGenerationRegistry& State = SaveGenerationRegistry();
	FScopeLock Lock(&State.Mutex);
	UnregisterSaveDirectory(State, Link);
}

const FString& FSaveGameGenerationReadLease::Directory() const
{
	return Link.Directory;
}

uint64 FSaveGameGenerationReadLease::GenerationIdentity() const
{
	return Link.Identity;
}

bool FSaveGameGenerationReadLease::Matches(const FString& InDirectory) const
{
	return !InDirectory.IsEmpty() && Link.Directory == CanonicalSaveGenerationDirectory(InDirectory);
}

bool FSaveGameGenerationReadLease::IsResourceAccountedFor(const FVoxelTerrainResources* InResources,
	const uint64 InWorldEpoch) const
{
	return ResourceLease && ResourceOwner == InResources && ResourceWorldEpoch == InWorldEpoch;
}

TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> FSaveGameGenerationReadLease::Acquire(const FString& InDirectory)
{
	TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> Lease;
	bool bCapacityBlocked = false;
	AcquireGranted(InDirectory, nullptr, {}, Lease, bCapacityBlocked);
	return Lease;
}

bool FSaveGameGenerationReadLease::AcquireGranted(const FString& InDirectory,
	const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
	const FVoxelGenerationConsumerKey& InConsumer,
	TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe>& OutLease, bool& bOutCapacityBlocked)
{
	bOutCapacityBlocked = false;
	if (InDirectory.IsEmpty() || OutLease || (InResources && !InConsumer.IsValid())) return false;
	FString Key = CanonicalSaveGenerationDirectory(InDirectory);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FSaveGameGenerationReadLease>() + Key.GetAllocatedSize();
	if (InResources && !InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input, Bytes, Lease))
	{
		bOutCapacityBlocked = true;
		return false;
	}
	FSaveGenerationRegistry& State = SaveGenerationRegistry();
	FScopeLock Lock(&State.Mutex);
	for (const auto* Link = State.Head; Link; Link = Link->Next)
	{
		if (Link->Access == SaveGameGeneration::EDirectoryAccess::Retiring &&
			(ContainsSaveGenerationDirectory(Link->Directory, Key) || ContainsSaveGenerationDirectory(Key, Link->Directory))) return false;
	}
	const uint64 Identity = ResolveSaveDirectoryIdentity(State, Key);
	auto* Reader = new FSaveGameGenerationReadLease(MoveTemp(Key), Identity, Lease, InResources.Get(), InConsumer.WorldEpoch);
	RegisterSaveDirectory(State, Reader->Link);
	if (InResources)
	{
		OutLease = TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe>(Reader,
			VoxelBoundedAllocation::TWorkerDeleter<FSaveGameGenerationReadLease>{MoveTemp(Lease)});
	}
	else OutLease = MakeShareable(Reader);
	return true;
}

bool FSaveGameGenerationReadLease::TryRetireDirectory(const FString& InDirectory, const FSaveGameGenerationWriteLease* InWriter)
{
    return TryRetireDirectory(InDirectory, InWriter, [&InDirectory]()
    {
        return IFileManager::Get().DeleteDirectory(*InDirectory, false, true);
    });
}

bool FSaveGameGenerationReadLease::TryRetireDirectory(const FString& InDirectory,
    const FSaveGameGenerationWriteLease* InWriter, TFunctionRef<bool()> InRetireAction)
{
	if (InDirectory.IsEmpty()) return false;
	SaveGameGeneration::FDirectoryLink Retiring;
	Retiring.Directory = CanonicalSaveGenerationDirectory(InDirectory);
	Retiring.Access = SaveGameGeneration::EDirectoryAccess::Retiring;
	if (InWriter && !ContainsSaveGenerationDirectory(InWriter->Link.Directory, Retiring.Directory)) return false;
	FSaveGenerationRegistry& State = SaveGenerationRegistry();
	{
		FScopeLock Lock(&State.Mutex);
		for (const auto* Link = State.Head; Link; Link = Link->Next)
		{
			if (!(ContainsSaveGenerationDirectory(Link->Directory, Retiring.Directory) ||
				ContainsSaveGenerationDirectory(Retiring.Directory, Link->Directory))) continue;
			if (Link->Access != SaveGameGeneration::EDirectoryAccess::Writer || !InWriter || Link != &InWriter->Link) return false;
		}
		RegisterSaveDirectory(State, Retiring);
	}
	// 登记真实栈节点保护原生删除，文件系统操作不持有注册表锁。
	const bool bDeleted = InRetireAction();
	{
		FScopeLock Lock(&State.Mutex);
		UnregisterSaveDirectory(State, Retiring);
		AdvanceSaveWriterReleaseVersion(State);
	}
	return bDeleted;
}

uint64 FSaveGameGenerationReadLease::GetPinCount(const FString& InDirectory)
{
	if (InDirectory.IsEmpty()) return 0;
	const FString Key = CanonicalSaveGenerationDirectory(InDirectory);
	FSaveGenerationRegistry& State = SaveGenerationRegistry();
	FScopeLock Lock(&State.Mutex);
	uint64 Pins = 0;
	for (const auto* Link = State.Head; Link; Link = Link->Next)
	{
		if (Link->Access == SaveGameGeneration::EDirectoryAccess::Reader && ContainsSaveGenerationDirectory(Key, Link->Directory))
		{
			check(Pins != MAX_uint64);
			++Pins;
		}
	}
	return Pins;
}

FSaveGameGenerationWriteLease::FSaveGameGenerationWriteLease(FString InDirectory, const uint64 InIdentity,
	const FGuid& InTransactionId, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease)
	: ResourceLease(MoveTemp(InLease))
{
	Link.Directory = MoveTemp(InDirectory);
	Link.Identity = InIdentity;
	Link.Transaction = InTransactionId;
	Link.Access = SaveGameGeneration::EDirectoryAccess::Writer;
}

FSaveGameGenerationWriteLease::~FSaveGameGenerationWriteLease()
{
	FSaveGenerationRegistry& State = SaveGenerationRegistry();
	FScopeLock Lock(&State.Mutex);
	UnregisterSaveDirectory(State, Link);
	AdvanceSaveWriterReleaseVersion(State);
}

bool FSaveGameGenerationWriteLease::Matches(const FString& InDirectory) const
{
	return !InDirectory.IsEmpty() && Link.Directory == CanonicalSaveGenerationDirectory(InDirectory);
}

bool FSaveGameGenerationWriteLease::Contains(const FString& InDirectory) const
{
	return !InDirectory.IsEmpty() && ContainsSaveGenerationDirectory(Link.Directory, CanonicalSaveGenerationDirectory(InDirectory));
}

const FGuid& FSaveGameGenerationWriteLease::TransactionId() const
{
	return Link.Transaction;
}

ESaveWorldWriteResult FSaveGameGenerationWriteLease::Acquire(const FString& InDirectory, const FGuid& InTransactionId,
	const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources, const FVoxelGenerationConsumerKey& InConsumer,
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe>& OutLease)
{
	if (InDirectory.IsEmpty() || !InTransactionId.IsValid() || OutLease || (InResources && !InConsumer.IsValid()))
		return ESaveWorldWriteResult::InvalidArgument;
	FString Key = CanonicalSaveGenerationDirectory(InDirectory);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FSaveGameGenerationWriteLease>() + Key.GetAllocatedSize();
	if (InResources && !InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input, Bytes, Lease))
		return ESaveWorldWriteResult::CapacityBlocked;
	FSaveGenerationRegistry& State = SaveGenerationRegistry();
	FScopeLock Lock(&State.Mutex);
	for (const auto* Link = State.Head; Link; Link = Link->Next)
	{
		if ((Link->Access == SaveGameGeneration::EDirectoryAccess::Writer || Link->Access == SaveGameGeneration::EDirectoryAccess::Retiring) &&
			(ContainsSaveGenerationDirectory(Link->Directory, Key) || ContainsSaveGenerationDirectory(Key, Link->Directory)))
			return ESaveWorldWriteResult::Busy;
	}
	const uint64 Identity = ResolveSaveDirectoryIdentity(State, Key);
	auto* Writer = new FSaveGameGenerationWriteLease(MoveTemp(Key), Identity, InTransactionId, Lease);
	RegisterSaveDirectory(State, Writer->Link);
	if (InResources)
	{
		OutLease = TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe>(Writer,
			VoxelBoundedAllocation::TWorkerDeleter<FSaveGameGenerationWriteLease>{MoveTemp(Lease)});
	}
	else OutLease = MakeShareable(Writer);
	return ESaveWorldWriteResult::Acquired;
}

uint64 FSaveGameGenerationWriteLease::ReleaseVersion()
{
	return SaveGenerationRegistry().WriterReleaseVersion.load(std::memory_order_acquire);
}

