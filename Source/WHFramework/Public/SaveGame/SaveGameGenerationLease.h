#pragma once

#include "CoreMinimal.h"

class FVoxelTerrainResources;
class FVoxelTerrainResourceLease;
struct FVoxelGenerationConsumerKey;
class FSaveGameGenerationWriteLease;

namespace SaveGameGeneration
{
	enum class EDirectoryAccess : uint8
	{
		None,
		Reader,
		Writer,
		Retiring
	};

	struct FDirectoryLink
	{
		FString Directory;
		FGuid Transaction;
		FDirectoryLink* Previous = nullptr;
		FDirectoryLink* Next = nullptr;
		uint64 Identity = 0;
		EDirectoryAccess Access = EDirectoryAccess::None;
	};
}

enum class ESaveWorldWriteResult : uint8
{
	None,
	Acquired,
	Busy,
	CapacityBlocked,
	InvalidArgument
};

/** Pins one immutable committed directory through read and decode completion. */
class WHFRAMEWORK_API FSaveGameGenerationReadLease
{
public:
	~FSaveGameGenerationReadLease();
	FSaveGameGenerationReadLease(const FSaveGameGenerationReadLease&) = delete;
	FSaveGameGenerationReadLease& operator=(const FSaveGameGenerationReadLease&) = delete;

	const FString& Directory() const;
	uint64 GenerationIdentity() const;
	bool Matches(const FString& InDirectory) const;
	bool IsResourceAccountedFor(const FVoxelTerrainResources* InResources, uint64 InWorldEpoch) const;
	const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& GetResourceLease() const { return ResourceLease; }

	static TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> Acquire(const FString& InDirectory);
	static bool AcquireGranted(const FString& InDirectory,
		const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe>& OutLease, bool& bOutCapacityBlocked);
	static bool TryRetireDirectory(const FString& InDirectory, const FSaveGameGenerationWriteLease* InWriter = nullptr);
	static uint64 GetPinCount(const FString& InDirectory);

private:
	FSaveGameGenerationReadLease(FString InDirectory, uint64 InIdentity,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease,
		const FVoxelTerrainResources* InResources, uint64 InWorldEpoch);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	SaveGameGeneration::FDirectoryLink Link;
	const FVoxelTerrainResources* ResourceOwner = nullptr;
	uint64 ResourceWorldEpoch = 0;
};

// 同一原生目录注册表独占存档槽的修改，允许读取已有不可变文件代。
class WHFRAMEWORK_API FSaveGameGenerationWriteLease
{
public:
	~FSaveGameGenerationWriteLease();
	FSaveGameGenerationWriteLease(const FSaveGameGenerationWriteLease&) = delete;
	FSaveGameGenerationWriteLease& operator=(const FSaveGameGenerationWriteLease&) = delete;
	bool Matches(const FString& InDirectory) const;
	bool Contains(const FString& InDirectory) const;
	const FGuid& TransactionId() const;
	static ESaveWorldWriteResult Acquire(const FString& InDirectory, const FGuid& InTransactionId,
		const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe>& OutLease);
	static uint64 ReleaseVersion();

private:
	friend class FSaveGameGenerationReadLease;
	FSaveGameGenerationWriteLease(FString InDirectory, uint64 InIdentity, const FGuid& InTransactionId,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	SaveGameGeneration::FDirectoryLink Link;
};
