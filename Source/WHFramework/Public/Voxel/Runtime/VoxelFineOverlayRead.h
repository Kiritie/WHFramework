#pragma once

#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelOverlaySnapshot.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Save/VoxelRegionStore.h"
#include "Voxel/Generation/VoxelWorldManifest.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

struct FVoxelFineOverlayCapture;

// 几何只保留区域版本凭据；采样正文和文件读取租约可以在网格完成后释放。
struct WHFRAMEWORK_API FVoxelFineOverlayVersionProof
{
	uint64 WorldEpoch = 0;
	FGuid DirectoryEpoch;
	TMap<FIntVector, uint64> RegionSequences;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
};

// RegionStore 提供具体实现；持有不可变目录根与精确文件代的 RAII 租约。
class WHFRAMEWORK_API IVoxelFineOverlayReadLeaseSet
{
public:
	virtual ~IVoxelFineOverlayReadLeaseSet() = default;
	virtual uint64 GetWorldEpoch() const = 0;
	virtual bool IsRemoteDirectory() const { return false; }
	virtual bool ValidateCapture(const FVoxelFineOverlayCapture& InCapture, FString& OutError) const = 0;
	virtual bool ValidateRead(const FVoxelRegionReadView& InRead, TConstArrayView<uint8> InBytes,
		uint64 InDecodedRevision, FString& OutError) const = 0;
	virtual uint64 GetMetadataBytes() const = 0;
	virtual void VisitResourceSources(const FVoxelTerrainResources& InResources,
		FVoxelGenerationRequiredSourceScope::FVisit InVisit) const = 0;
};

struct WHFRAMEWORK_API FVoxelFineOverlayCapture
{
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	uint64 WorldEpoch = 0;
	FVoxelGenerationBounds Bounds;
	FVoxelWorldManifest Manifest;
	TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;
	FVoxelOverlaySnapshotSet Resident;
	TMap<FIntVector, uint64> ContentRevisions;
	FGuid ReplicaDirectoryEpoch;
	TMap<FIntVector, uint64> ReplicaRegionSequences;
	TSharedPtr<const FVoxelFineOverlayVersionProof, ESPMode::ThreadSafe> VersionProof;
	TArray<FVoxelRegionReadView> Reads;
	TSharedPtr<const IVoxelFineOverlayReadLeaseSet, ESPMode::ThreadSafe> ReadLeaseSet;

	uint64 AllocatedBytes() const;
	void VisitResourceSources(const FVoxelTerrainResources& InResources,
		FVoxelGenerationRequiredSourceScope::FVisit InVisit) const;
};

struct WHFRAMEWORK_API FVoxelFineOverlayReadResult
{
	uint64 ResourceAccountedBytes = 0;
	uint64 WorldEpoch = 0;
	FVoxelGenerationBounds Bounds;
	TMap<FIntVector, uint64> ContentRevisions;
	FGuid ReplicaDirectoryEpoch;
	TMap<FIntVector, uint64> ReplicaRegionSequences;
	TSharedPtr<const FVoxelFineOverlayVersionProof, ESPMode::ThreadSafe> VersionProof;
	TSharedPtr<const FVoxelOverlaySnapshotSet, ESPMode::ThreadSafe> Snapshot;
	TSharedPtr<const IVoxelFineOverlayReadLeaseSet, ESPMode::ThreadSafe> ReadLeaseSet;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;

	uint64 AllocatedBytes() const;
};

enum class EVoxelFineOverlayReadState : uint8
{
	None = 0,
	Working,
	Complete,
	Canceled,
	Failed,
	Taken
};

// 只执行已持租约的读取；捕获入口统一为 RegionStore::CapturePinnedRead。
class WHFRAMEWORK_API FVoxelFineOverlayRead
{
public:
	bool Initialize(FVoxelFineOverlayCapture&& InCapture, FString& OutError);
	bool Advance(int32 InMaximumReads, const TAtomic<bool>& InCancel, bool& bOutComplete, FString& OutError);
	bool TakeResult(FVoxelFineOverlayReadResult& OutResult);
	uint64 GetWorldEpoch() const;
	uint64 AllocatedBytes() const;
	EVoxelFineOverlayReadState GetState() const;
	void VisitResourceSources(const FVoxelTerrainResources& InResources,
		FVoxelGenerationRequiredSourceScope::FVisit InVisit) const;

private:
	bool Fail(EVoxelFineOverlayReadState InState, FString InError, FString& OutError);
	FVoxelFineOverlayCapture Capture;
	TSharedPtr<FVoxelOverlaySnapshotSet, ESPMode::ThreadSafe> Snapshot;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PendingReadLease;
	TArray<uint8> PendingBytes;
	FVoxelDeltaDecoder Decoder;
	int32 NextRead = 0;
	EVoxelFineOverlayReadState State = EVoxelFineOverlayReadState::None;
	FString Failure;
};
