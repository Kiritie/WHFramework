#pragma once

#include "CoreMinimal.h"
#include "Voxel/Network/VoxelOverlayReplica.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

struct WHFRAMEWORK_API FVoxelOverlayAuthorityIdentity
{
	uint64 WorldEpoch = 0;
	FVoxelOverlayHash Manifest;
	FGuid DirectoryEpoch;
	FGuid Subscription;
	FGuid Ticket;
	uint64 Cut = 0;
};

struct WHFRAMEWORK_API FVoxelOverlayAuthorityRegionCapture
{
	FVoxelOverlayRegionHead Head;
	FVoxelFineOverlayCapture Capture;
};

struct WHFRAMEWORK_API FVoxelOverlayAuthorityDirectory
{
	FVoxelOverlayRegionHead Head;
	TArray<FVoxelOverlayDirectoryEntry> Entries;
	FVoxelOverlayHash SnapshotHash;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
};

struct WHFRAMEWORK_API FVoxelOverlayAuthorityDirectoryRoot
{
	FVoxelOverlayAuthorityIdentity Identity;
	TMap<FIntVector, TSharedPtr<const FVoxelOverlayAuthorityDirectory, ESPMode::ThreadSafe>> Regions;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
};

struct WHFRAMEWORK_API FVoxelOverlayWirePayload
{
	TArray<uint8> Bytes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
};

struct WHFRAMEWORK_API FVoxelOverlayAuthorityRegionBody
{
	TSharedPtr<const FVoxelOverlayAuthorityDirectory, ESPMode::ThreadSafe> Directory;
	TSharedPtr<const FVoxelOverlaySnapshotSet, ESPMode::ThreadSafe> Snapshot;
	uint32 TotalBytes = 0;
};

struct WHFRAMEWORK_API FVoxelOverlayAuthorityPrepared final : FVoxelTaskCustomPayload
{
	TSharedPtr<const FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> Root;
	TArray<FVoxelOverlayAuthorityRegionBody> Bodies;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	virtual uint64 GetAllocatedBytes() const override;
	virtual uint64 GetResourceAccountedBytes() const override;
};

// GT 捕获固定切点；Worker 每步读取一份文件或编码一个 Section，不查询可变 Runtime。
class WHFRAMEWORK_API FVoxelOverlayAuthoritySnapshotBuild
{
public:
	FVoxelOverlayAuthoritySnapshotBuild(const FVoxelOverlayAuthorityIdentity& InIdentity,
		TArray<FVoxelOverlayAuthorityRegionCapture>&& InRegions,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InCaptureLease);
	~FVoxelOverlayAuthoritySnapshotBuild();
	FVoxelTaskResult Advance(const TAtomic<bool>& InCancel);
	bool IsCapacityBlocked() const;
	uint64 GetCapacityVersion() const;

private:
	struct FState;
	TUniquePtr<FState> State;
};

struct WHFRAMEWORK_API FVoxelOverlayEncodedMessage
{
	EVoxelMessage Kind = EVoxelMessage::None;
	TArray<uint8> Bytes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
};

// 编码器每步产生一个有界消息。所有 Region 共享一次 Head 向量声明。
class WHFRAMEWORK_API FVoxelOverlaySnapshotEncoder
{
public:
	FVoxelOverlaySnapshotEncoder(TSharedRef<const FVoxelOverlayAuthorityPrepared, ESPMode::ThreadSafe> InSnapshot,
		TArray<FVoxelOverlayRegionHead>&& InCatchupHeads,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	bool Advance(FVoxelOverlayEncodedMessage& OutMessage, bool& bOutComplete, FString& OutError);

private:
	TSharedRef<const FVoxelOverlayAuthorityPrepared, ESPMode::ThreadSafe> Snapshot;
	TArray<FVoxelOverlayRegionHead> CatchupHeads;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TSharedPtr<const FVoxelOverlayWirePayload, ESPMode::ThreadSafe> ActivePayload;
	FGuid SnapshotId = FGuid::NewGuid();
	int32 RegionCursor = 0;
	int32 DirectoryPage = 0;
	int32 PayloadCursor = 0;
	int32 PayloadPart = 0;
	uint8 Phase = 0;
};

struct WHFRAMEWORK_API FVoxelOverlayAuthorityEditCapture
{
	FGuid Transaction;
	uint64 Cut = 0;
	TArray<FVoxelOverlayRegionHead> Heads;
	TArray<TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>> Overlays;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
};

struct WHFRAMEWORK_API FVoxelOverlayAuthorityDelta final : FVoxelTaskCustomPayload
{
	TSharedPtr<const FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> Root;
	FGuid Transaction;
	uint64 Cut = 0;
	TArray<FVoxelOverlayRegionHead> Heads;
	TSharedPtr<const FVoxelOverlayWirePayload, ESPMode::ThreadSafe> Payload;
	FVoxelOverlayHash Hash;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	virtual uint64 GetAllocatedBytes() const override;
	virtual uint64 GetResourceAccountedBytes() const override;
};

class WHFRAMEWORK_API FVoxelOverlayAuthorityDeltaBuild
{
public:
	FVoxelOverlayAuthorityDeltaBuild(TSharedRef<const FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> InRoot,
		FVoxelOverlayAuthorityEditCapture&& InEdit,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	~FVoxelOverlayAuthorityDeltaBuild();
	FVoxelTaskResult Advance(const TAtomic<bool>& InCancel);
	bool IsCapacityBlocked() const;
	uint64 GetCapacityVersion() const;

private:
	struct FState;
	TUniquePtr<FState> State;
};

class WHFRAMEWORK_API FVoxelOverlayDeltaEncoder
{
public:
	FVoxelOverlayDeltaEncoder(TSharedRef<const FVoxelOverlayAuthorityDelta, ESPMode::ThreadSafe> InDelta,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	bool Advance(FVoxelOverlayEncodedMessage& OutMessage, bool& bOutComplete, FString& OutError);

private:
	TSharedRef<const FVoxelOverlayAuthorityDelta, ESPMode::ThreadSafe> Delta;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	uint32 Part = 0;
	bool bHeaderSent = false;
};
