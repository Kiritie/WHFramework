#pragma once
#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
class FSaveGameStorage;
enum class EVoxelRegionRead : uint8
{
	Missing,
	Loaded,
	Failed
};
struct WHFRAMEWORK_API FVoxelRegionReadView
{
	FString SourceDirectory;
	FVoxelSectionKey Key;
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
	TMap<FVoxelSectionKey, FVoxelRegionOperation> Operations;
};
class WHFRAMEWORK_API FVoxelRegionStore
{
public:
	void SetSource(const FGuid& SaveId, int32 Generation, FSaveGameStorage* Storage);
	void Reset()
	{
		SourceDirectory.Reset();
	}
	const FString& GetSourceDirectory() const
	{
		return SourceDirectory;
	}
	void AdvanceSource(const FString& CommittedDirectory)
	{
		SourceDirectory = CommittedDirectory;
	}
	FVoxelRegionReadView CaptureRead(const FVoxelSectionKey& Key) const;
	static EVoxelRegionRead Read(const FVoxelRegionReadView& View, TArray<uint8>& Out, FString& Error);
	static bool StageSection(FVoxelRegionWritePlan& Plan, const FVoxelSectionKey& Key, TArray<uint8>&& Bytes);
	static void StageDelete(FVoxelRegionWritePlan& Plan, const FVoxelSectionKey& Key);
	static bool WritePendingRegions(const FVoxelRegionWritePlan& Plan, const FString& TempGenerationDirectory, FString& Error);

private:
	FString SourceDirectory;
};
