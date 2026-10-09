#pragma once

#include "CoreMinimal.h"
#include "Voxel/Network/VoxelOverlayReplica.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

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
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FVoxelOverlayRegionHead Head;
	TArray<FVoxelOverlayDirectoryEntry> Entries;
	FVoxelOverlayHash SnapshotHash;
};

struct WHFRAMEWORK_API FVoxelOverlayAuthorityDirectoryRoot
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FVoxelOverlayAuthorityIdentity Identity;
	TMap<FIntVector, TSharedPtr<const FVoxelOverlayAuthorityDirectory, ESPMode::ThreadSafe>> Regions;
};

struct WHFRAMEWORK_API FVoxelOverlayWirePayload
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	TArray<uint8> Bytes;
};

struct WHFRAMEWORK_API FVoxelOverlayAuthorityRegionBody
{
	TSharedPtr<const FVoxelOverlayAuthorityDirectory, ESPMode::ThreadSafe> Directory;
	TSharedPtr<const FVoxelOverlaySnapshotSet, ESPMode::ThreadSafe> Snapshot;
	uint32 TotalBytes = 0;
};

struct WHFRAMEWORK_API FVoxelOverlayAuthorityPrepared final : FVoxelTaskCustomPayload
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	TSharedPtr<const FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> Root;
	TArray<FVoxelOverlayAuthorityRegionBody> Bodies;
	virtual uint64 GetAllocatedBytes() const override;
	virtual uint64 GetResourceAccountedBytes() const override;
};

// GT 捕获固定切点；Worker 每步读取一份文件或编码一个 Section，不查询可变 Runtime。
class WHFRAMEWORK_API FVoxelOverlayAuthoritySnapshotBuild
{
private:
	struct FConstructionKey { explicit FConstructionKey() = default; };
public:
	static uint64 NativeInputBytes();
	static void VisitCaptureSources(const FVoxelFineOverlayCapture& InCapture, const FVoxelTerrainResources& InResources,
		FVoxelGenerationRequiredSourceScope::FVisit InVisit);
	static EVoxelTerrainResourceAdmission Create(const FVoxelOverlayAuthorityIdentity& InIdentity,
		TArray<FVoxelOverlayAuthorityRegionCapture>&& InRegions,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InCaptureLease,
		TSharedPtr<FVoxelOverlayAuthoritySnapshotBuild, ESPMode::ThreadSafe>& OutBuild, FString& OutError);
	FVoxelOverlayAuthoritySnapshotBuild(FConstructionKey,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InNativeLease,
		const FVoxelOverlayAuthorityIdentity& InIdentity,
		TArray<FVoxelOverlayAuthorityRegionCapture>&& InRegions,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InCaptureLease);
	~FVoxelOverlayAuthoritySnapshotBuild();
	FVoxelTaskResult Advance(const TAtomic<bool>& InCancel);
	bool IsCapacityBlocked() const;
	uint64 GetCapacityVersion() const;
	FVoxelGenerationConsumerKey GetConsumer() const;

private:
	struct FState;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> NativeLease;
	TUniquePtr<FState> State;
};

struct WHFRAMEWORK_API FVoxelOverlayEncodedMessage
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	EVoxelMessage Kind = EVoxelMessage::None;
	TArray<uint8> Bytes;

	FVoxelOverlayEncodedMessage() = default;
	FVoxelOverlayEncodedMessage(FVoxelOverlayEncodedMessage&&) = default;
	FVoxelOverlayEncodedMessage& operator=(FVoxelOverlayEncodedMessage&& InOther)
	{
		if (this == &InOther) return *this;
		Bytes = MoveTemp(InOther.Bytes);
		ResourceLease = MoveTemp(InOther.ResourceLease);
		Kind = InOther.Kind;
		return *this;
	}
	FVoxelOverlayEncodedMessage(const FVoxelOverlayEncodedMessage&) = delete;
	FVoxelOverlayEncodedMessage& operator=(const FVoxelOverlayEncodedMessage&) = delete;
};

// 编码器每步产生一个有界消息。所有 Region 共享一次 Head 向量声明。
class WHFRAMEWORK_API FVoxelOverlaySnapshotEncoder
{
private:
	struct FConstructionKey { explicit FConstructionKey() = default; };
public:
	static void VisitSnapshotSources(const FVoxelOverlayAuthorityPrepared& InSnapshot, const FVoxelTerrainResources& InResources,
		FVoxelGenerationRequiredSourceScope::FVisit InVisit);
	static EVoxelTerrainResourceAdmission Create(TSharedRef<const FVoxelOverlayAuthorityPrepared, ESPMode::ThreadSafe> InSnapshot,
		TArray<FVoxelOverlayRegionHead>&& InCatchupHeads,
		const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InHeadLease,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelOverlaySnapshotEncoder, ESPMode::ThreadSafe>& OutEncoder, FString& OutError);
	FVoxelOverlaySnapshotEncoder(FConstructionKey, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InNativeLease,
		TSharedRef<const FVoxelOverlayAuthorityPrepared, ESPMode::ThreadSafe> InSnapshot,
		TArray<FVoxelOverlayRegionHead>&& InCatchupHeads, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InHeadLease,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, const FVoxelGenerationConsumerKey& InConsumer);
	bool Advance(FVoxelOverlayEncodedMessage& OutMessage, bool& bOutComplete, FString& OutError);
	FVoxelGenerationConsumerKey GetConsumer() const { return Consumer; }
	void VisitResourceSources(const FVoxelTerrainResources& InResources, FVoxelGenerationRequiredSourceScope::FVisit InVisit) const;

private:
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> NativeLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> HeadLease;
	TSharedRef<const FVoxelOverlayAuthorityPrepared, ESPMode::ThreadSafe> Snapshot;
	TArray<FVoxelOverlayRegionHead> CatchupHeads;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
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
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FGuid Transaction;
	uint64 Cut = 0;
	TArray<FVoxelOverlayRegionHead> Heads;
	TArray<TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>> Overlays;
};

struct WHFRAMEWORK_API FVoxelOverlayAuthorityDelta final : FVoxelTaskCustomPayload
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	TSharedPtr<const FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> Root;
	FGuid Transaction;
	uint64 Cut = 0;
	TArray<FVoxelOverlayRegionHead> Heads;
	TSharedPtr<const FVoxelOverlayWirePayload, ESPMode::ThreadSafe> Payload;
	FVoxelOverlayHash Hash;
	virtual uint64 GetAllocatedBytes() const override;
	virtual uint64 GetResourceAccountedBytes() const override;
};

class WHFRAMEWORK_API FVoxelOverlayAuthorityDeltaBuild
{
private:
	struct FConstructionKey { explicit FConstructionKey() = default; };
public:
	static uint64 NativeInputBytes();
	static void VisitRootSources(const FVoxelOverlayAuthorityDirectoryRoot& InRoot, const FVoxelTerrainResources& InResources,
		FVoxelGenerationRequiredSourceScope::FVisit InVisit);
	static void VisitEditSources(const FVoxelOverlayAuthorityEditCapture& InEdit, const FVoxelTerrainResources& InResources,
		FVoxelGenerationRequiredSourceScope::FVisit InVisit);
	static EVoxelTerrainResourceAdmission Create(
		TSharedRef<const FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> InRoot,
		FVoxelOverlayAuthorityEditCapture&& InEdit,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelOverlayAuthorityDeltaBuild, ESPMode::ThreadSafe>& OutBuild, FString& OutError);
	FVoxelOverlayAuthorityDeltaBuild(FConstructionKey,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InNativeLease,
		TSharedRef<const FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> InRoot,
		FVoxelOverlayAuthorityEditCapture&& InEdit,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	~FVoxelOverlayAuthorityDeltaBuild();
	FVoxelTaskResult Advance(const TAtomic<bool>& InCancel);
	bool IsCapacityBlocked() const;
	uint64 GetCapacityVersion() const;
	FVoxelGenerationConsumerKey GetConsumer() const;

private:
	struct FState;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> NativeLease;
	TUniquePtr<FState> State;
};

class WHFRAMEWORK_API FVoxelOverlayDeltaEncoder
{
private:
	struct FConstructionKey { explicit FConstructionKey() = default; };
public:
	static void VisitDeltaSources(const FVoxelOverlayAuthorityDelta& InDelta, const FVoxelTerrainResources& InResources,
		FVoxelGenerationRequiredSourceScope::FVisit InVisit);
	static EVoxelTerrainResourceAdmission Create(TSharedRef<const FVoxelOverlayAuthorityDelta, ESPMode::ThreadSafe> InDelta,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelOverlayDeltaEncoder, ESPMode::ThreadSafe>& OutEncoder, FString& OutError);
	FVoxelOverlayDeltaEncoder(FConstructionKey, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InNativeLease,
		TSharedRef<const FVoxelOverlayAuthorityDelta, ESPMode::ThreadSafe> InDelta,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, const FVoxelGenerationConsumerKey& InConsumer);
	bool Advance(FVoxelOverlayEncodedMessage& OutMessage, bool& bOutComplete, FString& OutError);
	FVoxelGenerationConsumerKey GetConsumer() const { return Consumer; }
	void VisitResourceSources(const FVoxelTerrainResources& InResources, FVoxelGenerationRequiredSourceScope::FVisit InVisit) const;

private:
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> NativeLease;
	TSharedRef<const FVoxelOverlayAuthorityDelta, ESPMode::ThreadSafe> Delta;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	uint32 Part = 0;
	bool bHeaderSent = false;
};
