#pragma once

#include "CoreMinimal.h"
#include "SaveGame/SaveGameGenerationLease.h"

class FSaveGameStorage;
class FVoxelTerrainResources;
class FVoxelTerrainResourceLease;
struct FVoxelGenerationConsumerKey;
struct FVoxelBinaryBuffer;
class FVoxelTaskScheduler;
struct FVoxelRegionDirectoryLoadAttempt;
struct FVoxelFineOverlayCapture;
class IVoxelFineOverlayReadLeaseSet;

static constexpr uint32 VoxelRegionFileVersion = 3;

struct WHFRAMEWORK_API FVoxelRegionFileHeader
{
	uint32 Magic = 0;
	uint32 Version = VoxelRegionFileVersion;
	uint32 EntryCount = 0;
	uint64 RegionRevision = 0;
	uint64 ModifiedMask[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
};

enum class EVoxelRegionRead : uint8
{
	Missing,
	Loaded,
	Failed
};

struct WHFRAMEWORK_API FVoxelRegionReadView
{
	FString SourceDirectory;
	FIntVector Section = FIntVector::ZeroValue;
	TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> GenerationLease;
	uint64 ExpectedSectionRevision = 0;
	uint64 ExpectedRegionRevision = 0;
	uint64 ExpectedRecordOffset = 0;
	uint32 ExpectedRecordBytes = 0;
	uint32 ExpectedRecordCrc = 0;
};

struct WHFRAMEWORK_API FVoxelRegionOperation
{
	bool bDelete = false;
	TSharedPtr<const TArray<uint8>, ESPMode::ThreadSafe> Bytes;
};

struct WHFRAMEWORK_API FVoxelRegionRecordDescriptor
{
	uint64 RegionRevision = 0;
	uint64 Offset = 0;
	uint32 Bytes = 0;
	uint32 Crc = 0;
};

struct WHFRAMEWORK_API FVoxelRegionDirectorySnapshot
{
	struct FSpatialNode
	{
		int32 Children[8] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};
		FIntVector Region = FIntVector::ZeroValue;
	};
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	TMap<FIntVector, FVoxelRegionRecordDescriptor> Records;
	TMap<FIntVector, FVoxelRegionFileHeader> Regions;
	TMap<FIntPoint, uint32> SceneColumns;
	TArray<FIntVector> RegionOrder;
	TArray<FSpatialNode> SpatialNodes;
	bool VisitRegions(const FIntVector& InMinimum, const FIntVector& InMaximum,
		TFunctionRef<void(const FIntVector&, const FVoxelRegionFileHeader&)> InVisit,
		const TAtomic<bool>* InCancel = nullptr) const;
};

struct WHFRAMEWORK_API FVoxelRegionWritePlan
{
	FGuid TransactionId;
	FString SourceDirectory;
	TMap<FIntVector, FVoxelRegionOperation> Operations;
};

class WHFRAMEWORK_API FVoxelRegionStore
{
public:
	FVoxelRegionStore();
	~FVoxelRegionStore();
	FVoxelRegionStore(const FVoxelRegionStore&) = delete;
	FVoxelRegionStore& operator=(const FVoxelRegionStore&) = delete;
	void SetSource(const FGuid& InSaveId, int32 InGeneration, FSaveGameStorage* InStorage,
		bool bInLoadAsynchronously = false);
	bool IsDirectoryReady() const;
	void AdvanceDirectoryLoad(FVoxelTaskScheduler& InScheduler, uint64 InWorldEpoch,
		TFunction<void(TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe>)> InOnReady);
	void Reset();
	const FString& GetSourceDirectory() const;
	const TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe>& GetSourceLease() const;
	const TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe>& GetDirectorySnapshot() const;
	const FString& GetDirectoryError() const;
	void AdvanceSource(const FString& InCommittedDirectory);
	bool AdvanceSourcePrepared(const FString& InCommittedDirectory,
		TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe> InDirectory, FString& OutError,
		TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> InPreparedLease = {});
	static bool BuildDirectory(const FString& InDirectory,
		const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe>& OutDirectory, FString& OutError,
		const TAtomic<bool>* InCancel = nullptr, const TAtomic<bool>* InDetached = nullptr);
	FVoxelRegionReadView CaptureRead(const FIntVector& InSection) const;
	bool CapturePinnedRead(const FIntVector& InSection, uint64 InExpectedRevision,
		FVoxelRegionReadView& OutRead, FString& OutError) const;
	static void ReleaseReadLease(FVoxelRegionReadView& InRead);
	bool TryRetireFileGeneration(const FString& InDirectory) const;
	static TSharedRef<const IVoxelFineOverlayReadLeaseSet, ESPMode::ThreadSafe> CreateOverlayReadLeaseSet(
		const FVoxelFineOverlayCapture& InCapture);
	static uint64 OverlayReadLeaseBytes(int32 InPinnedSections, int32 InReads,
		uint64 InReadPathBytes, int32 InContentRevisions);
	EVoxelRegionRead ReadSection(
		const FIntVector& InSection,
		TArray<uint8>& OutBytes,
		FString& OutError) const;
	bool ReadChangeHeader(
		const FIntVector& InRegion,
		uint64& OutRevision,
		TArray<uint64>& OutModifiedMask,
		FString& OutError) const;
	bool ScanChangeHeaders(
		TFunctionRef<void(const FIntVector&, uint64, const TArray<uint64>&)> InVisit,
		FString& OutError) const;

	static EVoxelRegionRead Read(
		const FVoxelRegionReadView& InView,
		TArray<uint8>& OutBytes,
		FString& OutError);
	static uint64 ReadWorkingBytes(const FVoxelRegionReadView& InView);
	static bool StageSection(
		FVoxelRegionWritePlan& InPlan,
		const FIntVector& InSection,
		TArray<uint8>&& InBytes);
	static void StageDelete(FVoxelRegionWritePlan& InPlan, const FIntVector& InSection);
	static bool WritePendingRegions(
		const FVoxelRegionWritePlan& InPlan,
		const FString& InTemporaryGenerationDirectory,
		FString& OutError);
	// Worker按记录流式改写一个Region，不累计全部压缩增量。
	static bool WritePendingRegion(const FIntVector& InRegion, const FString& InSourceDirectory,
		const FString& InTemporaryGenerationDirectory,
		const TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe>& InSourceDirectoryRoot,
		const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TFunctionRef<bool(const FIntVector&)> InHasReplacement,
		TFunctionRef<bool(const FIntVector&, FVoxelBinaryBuffer&, FString&)> InEncodeReplacement,
		FString& OutError);

private:
	void DetachDirectoryLoad();
	bool LoadDirectory(FString& OutError);
	TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe> Directory;
	FString DirectoryError;
	FString SourceDirectory;
	TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> SourceLease;
	FGuid DirectoryOwnerId = FGuid::NewGuid();
	uint64 DirectoryOwnerIncarnation = 1;
	uint64 DirectoryTaskSerial = 0;
	double DirectoryFirstRunnable = -1.0;
	bool bDirectoryReady = true;
	TSharedPtr<FVoxelRegionDirectoryLoadAttempt, ESPMode::ThreadSafe> DirectoryLoadAttempt;
};
