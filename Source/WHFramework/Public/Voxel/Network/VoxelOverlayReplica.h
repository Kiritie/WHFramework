#pragma once

#include "CoreMinimal.h"
#include "Voxel/Network/VoxelNetworkTypes.h"
#include "Voxel/Runtime/VoxelFineOverlayRead.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

struct WHFRAMEWORK_API FVoxelOverlayHash
{
	uint8 Bytes[32] = {};
	bool operator==(const FVoxelOverlayHash& InOther) const;
	bool operator!=(const FVoxelOverlayHash& InOther) const;
	bool IsZero() const;
	static bool Compute(TConstArrayView<uint8> InBytes, FVoxelOverlayHash& OutHash);
};

enum class EVoxelOverlayReplicaState : uint8
{
	Unknown,
	DirectoryKnown,
	SnapshotReceiving,
	CatchingUp,
	Complete,
	Gap,
	ResyncRequired
};

struct WHFRAMEWORK_API FVoxelOverlayRegionHead
{
	FIntVector Region = FIntVector::ZeroValue;
	uint64 PreviousSequence = 0;
	uint64 Sequence = 0;
	FVoxelOverlayHash BeforeHash;
	FVoxelOverlayHash Hash;
	bool operator==(const FVoxelOverlayRegionHead& InOther) const = default;
};

struct WHFRAMEWORK_API FVoxelOverlayDirectoryEntry
{
	FIntVector Section = FIntVector::ZeroValue;
	uint64 Revision = 0;
	uint64 BeforeRevision = 0;
	FVoxelOverlayHash BeforeHash;
	FVoxelOverlayHash PayloadHash;
	uint32 PayloadBytes = 0;
	bool bTombstone = false;
	bool operator==(const FVoxelOverlayDirectoryEntry& InOther) const = default;
};

// 目录、正文分片和增量共享同一完整身份；消息摘要包含固定序列化的全部字段。
struct WHFRAMEWORK_API FVoxelOverlayRegionMessage
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	uint64 WorldEpoch = 0;
	FVoxelOverlayHash ManifestIdentity;
	FGuid SubscriptionId;
	FGuid CoverageTicket;
	FGuid DirectoryEpoch;
	FGuid SnapshotId;
	FGuid AuthorityTransaction;
	FIntVector Region = FIntVector::ZeroValue;
	FIntVector Section = FIntVector::ZeroValue;
	uint64 AuthorityCut = 0;
	uint64 DirectoryRevision = 0;
	uint64 BaseSequence = 0;
	uint64 TargetSequence = 0;
	uint64 OverlayRevision = 0;
	uint32 Index = 0;
	uint32 Count = 0;
	uint32 EntryCount = 0;
	uint32 TotalBytes = 0;
	FVoxelOverlayHash DirectoryHash;
	FVoxelOverlayHash ContentHash;
	FVoxelOverlayHash WireDigest;
	TArray<FVoxelOverlayRegionHead> Heads;
	TArray<FVoxelOverlayDirectoryEntry> Entries;
	TArray<uint8> Payload;
	uint64 AllocatedBytes() const;
};

struct WHFRAMEWORK_API FVoxelOverlayRegionSnapshot
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FIntVector Region = FIntVector::ZeroValue;
	FGuid InitialCoverageTicket;
	FGuid DirectoryEpoch;
	uint64 DirectoryRevision = 0;
	uint64 Sequence = 0;
	uint64 AuthorityCut = 0;
	FVoxelOverlayHash DirectoryHash;
	TMap<FIntVector, FVoxelOverlayDirectoryEntry> Directory;
	TMap<FIntVector, TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>> Overlays;
};

class WHFRAMEWORK_API FVoxelOverlayRegionCodec
{
public:
	static constexpr int32 MaximumRegions = 512;
	static constexpr int32 MaximumPageEntries = 32;
	static constexpr int32 MaximumPartBytes = 16384;
	static constexpr int32 MaximumSnapshotBytes = 8 * 1024 * 1024;
	static constexpr int32 MaximumMessageBytes = MaximumPartBytes + MaximumRegions * 92 + MaximumPageEntries * 128 + 512;
	static FIntVector RegionOf(const FIntVector& InSection);
	static bool GatherRegions(const FVoxelGenerationBounds& InBounds, TArray<FIntVector>& OutRegions);
	static bool ManifestHash(const FVoxelWorldManifest& InManifest, FVoxelOverlayHash& OutHash);
	static bool Encode(EVoxelMessage InKind, const FVoxelOverlayRegionMessage& InMessage,
		TArray<uint8>& OutBytes);
	static bool EncodedByteCount(const FVoxelOverlayRegionMessage& InMessage, int32& OutBytes,
		int32 InPlannedHeadCount = INDEX_NONE);
	static bool Decode(EVoxelMessage InKind, TConstArrayView<uint8> InBytes,
		FVoxelOverlayRegionMessage& OutMessage, FString& OutError);
	static bool MeasureDecode(EVoxelMessage InKind, TConstArrayView<uint8> InBytes,
		uint64& OutBytes, FString& OutError);
	static bool DirectoryHash(TConstArrayView<FVoxelOverlayDirectoryEntry> InEntries,
		FVoxelOverlayHash& OutHash);
	static bool SnapshotHash(TConstArrayView<FVoxelOverlayDirectoryEntry> InEntries,
		FVoxelOverlayHash& OutHash);
	static bool EncodeOverlay(const FVoxelOverlaySnapshot& InOverlay,
		TArray<uint8>& OutBytes, FString& OutError);
	static bool DecodeOverlay(TConstArrayView<uint8> InBytes,
		FVoxelOverlaySnapshot& OutOverlay, FString& OutError);
	static bool OverlayCapacity(TConstArrayView<uint8> InBytes, uint64& OutBytes);
	static bool EncodeDelta(TConstArrayView<FVoxelOverlayDirectoryEntry> InEntries,
		TConstArrayView<TArray<uint8>> InPayloads, TArray<uint8>& OutBytes);
	static bool DecodeDelta(TConstArrayView<uint8> InBytes,
		TArray<FVoxelOverlayDirectoryEntry>& OutEntries, TArray<TArray<uint8>>& OutPayloads);
};

struct FVoxelOverlayReplicaRoot;
struct FVoxelOverlayReplicaReceiving;
struct FVoxelOverlayReplicaWork;
struct FVoxelOverlayReplicaLifetime;
struct FVoxelTaskRequest;
struct FVoxelTaskResult;
struct FVoxelTaskStamp;

struct WHFRAMEWORK_API FVoxelOverlayCommitNotification
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FGuid AuthorityTransaction;
	TArray<FIntVector> Sections;
	TArray<FVoxelOverlayRegionHead> Heads;
};

// GT 唯一写者；Worker 只消费捕获的 Region 强引用，不读取可变网络状态。
class WHFRAMEWORK_API FVoxelOverlayReplica
{
public:
	FVoxelOverlayReplica(uint64 InLocalEpoch,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources);
	~FVoxelOverlayReplica();
	void SetAuthority(uint64 InAuthorityEpoch, const FVoxelOverlayHash& InManifest,
		const FGuid& InDirectoryEpoch);
	void SetTaskQueue(TFunction<bool(FVoxelTaskRequest&&)> InQueue);
	void SetTaskBindingDomain(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	void SetTaskCanceler(TFunction<void(const FVoxelTaskStamp&)> InCancel);
	bool Receive(EVoxelMessage InKind, FVoxelOverlayRegionMessage&& InMessage, FString& OutError);
	EVoxelTerrainResourceAdmission TryExpectTicket(const FGuid& InSubscription, const FGuid& InTicket,
		TConstArrayView<FIntVector> InRegions, const FVoxelGenerationConsumerKey& InConsumer, FString& OutError);
#if WITH_DEV_AUTOMATION_TESTS
	bool ExpectTicket(const FGuid& InSubscription, const FGuid& InTicket,
		TConstArrayView<FIntVector> InRegions);
#endif
	void CancelTicket(const FGuid& InTicket);
	bool Advance(int32 InMaximumOperations, FString& OutError);
	bool Capture(const FVoxelGenerationBounds& InBounds, const FVoxelWorldManifest& InManifest,
		TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
		FVoxelFineOverlayCapture& OutCapture, FString& OutError) const;
	bool IsCurrent(const FVoxelFineOverlayReadResult& InRead) const;
	bool CaptureVersion(const FVoxelGenerationBounds& InBounds,
		TSharedPtr<const FVoxelFineOverlayVersionProof, ESPMode::ThreadSafe>& OutProof, FString& OutError,
		const FVoxelGenerationConsumerKey& InConsumer = {}) const;
	bool IsCurrent(const FVoxelFineOverlayVersionProof& InProof) const;
	bool LookupSection(const FIntVector& InSection, uint64& OutRevision,
		TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>& OutOverlay) const;
	const FVoxelOverlayRegionSnapshot* FindRegion(const FIntVector& InRegion) const;
	EVoxelOverlayReplicaState GetState(const FIntVector& InRegion) const;
	bool GetTicketHead(const FGuid& InTicket, const FIntVector& InRegion, FVoxelOverlayRegionHead& OutHead) const;
	void MarkGap(const FIntVector& InRegion);
	void Reset();
	uint64 GetAuthorityEpoch() const;
	uint64 GetProgressSerial() const;
	const FGuid& GetDirectoryEpoch() const;
	const FVoxelOverlayHash& GetManifestIdentity() const;
	TFunction<void(TSharedPtr<const FVoxelOverlayCommitNotification, ESPMode::ThreadSafe>)> OnCommitted;

private:
	friend struct FVoxelOverlayReplicaWork;
	bool QueuePreparation(const FGuid& InIdentity, bool bInDelta, FString& OutError);
	bool RetryPendingPreparation(FString& OutError);
	void CancelPendingPreparation();
	bool PrepareReceivingRegion(const FIntVector& InRegion, FString& OutError);
	void ApplyPreparation(TSharedPtr<FVoxelOverlayReplicaWork, ESPMode::ThreadSafe> InWork,
		FVoxelTaskResult&& InResult);
	bool TryCommitTicket(const FGuid& InTicket, FString& OutError);
	bool TryApplyDelta(const FGuid& InTransaction, FString& OutError);
	bool FailRegion(const FIntVector& InRegion, FString InReason, FString& OutError);
	TSharedPtr<const FVoxelOverlayCommitNotification, ESPMode::ThreadSafe> PrepareNotification(
		const FGuid& InTransaction, TArray<FIntVector>&& InSections, TConstArrayView<FVoxelOverlayRegionHead> InHeads, FString& OutError);
	EVoxelTerrainResourceAdmission GrantBody(EVoxelTerrainResourcePool InPool, uint64 InBytes,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease, FString& OutError, int32 InSplits = 0);
	uint64 LocalEpoch = 0;
	uint64 AuthorityEpoch = 0;
	uint64 NextSerial = 0;
	uint64 ProgressSerial = 0;
	FGuid OwnerId = FGuid::NewGuid();
	FGuid DirectoryEpoch;
	FVoxelOverlayHash ManifestIdentity;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<const FVoxelOverlayReplicaRoot, ESPMode::ThreadSafe> Root;
	TUniquePtr<FVoxelOverlayReplicaReceiving> Receiving;
	TSharedPtr<FVoxelOverlayReplicaWork, ESPMode::ThreadSafe> Preparing;
	TSharedPtr<FVoxelOverlayReplicaLifetime, ESPMode::ThreadSafe> Lifetime;
	TFunction<bool(FVoxelTaskRequest&&)> TaskQueue;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> TaskBindingResources;
	FVoxelGenerationConsumerKey TaskBindingConsumer;
	TOptional<FVoxelTaskRequest> PendingTask;
	uint64 BindingCapacityVersion = 0;
	bool bBindingCapacityBlocked = false;
	TFunction<void(const FVoxelTaskStamp&)> TaskCanceler;
	uint64 OwnerIncarnation = 1;
	bool bDetachedWorker = false;
	FVoxelGenerationConsumerKey BodyConsumer;
	EVoxelTerrainResourceAdmission BodyAdmission = EVoxelTerrainResourceAdmission::None;
	uint64 BodyCapacityVersion = 0;
};
