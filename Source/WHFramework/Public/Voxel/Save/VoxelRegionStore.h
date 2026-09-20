#pragma once

#include "CoreMinimal.h"

class FSaveGameStorage;

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
};

struct WHFRAMEWORK_API FVoxelRegionOperation
{
	bool bDelete = false;
	TSharedPtr<const TArray<uint8>, ESPMode::ThreadSafe> Bytes;
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
	void SetSource(const FGuid& InSaveId, int32 InGeneration, FSaveGameStorage* InStorage);
	void Reset();
	const FString& GetSourceDirectory() const;
	void AdvanceSource(const FString& InCommittedDirectory);
	FVoxelRegionReadView CaptureRead(const FIntVector& InSection) const;
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
	static bool StageSection(
		FVoxelRegionWritePlan& InPlan,
		const FIntVector& InSection,
		TArray<uint8>&& InBytes);
	static void StageDelete(FVoxelRegionWritePlan& InPlan, const FIntVector& InSection);
	static bool WritePendingRegions(
		const FVoxelRegionWritePlan& InPlan,
		const FString& InTemporaryGenerationDirectory,
		FString& OutError);

private:
	FString SourceDirectory;
};
