#pragma once

#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Save/VoxelRegionStore.h"
#include "Voxel/Generation/VoxelWorldManifest.h"
#include "Tasks/Task.h"

struct FVoxelSceneSaveCapture;

struct WHFRAMEWORK_API FVoxelWorldSaveCompletion
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> OwnerLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> TargetLease;
	FGuid ExpectedSaveId;
	TSharedPtr<const FString, ESPMode::ThreadSafe> ExpectedStorageRoot;
	FGuid TransactionId;
	uint64 WorldEpoch = 0;
	FGuid TargetSaveId;
	int32 TargetGeneration = 0;
	TSharedPtr<const FString, ESPMode::ThreadSafe> TargetDirectory;
	TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> TargetReadLease;
	TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe> Directory;
	bool bSceneFilesWritten = false;
	bool bFilesFlushed = false;
	bool bGenerationCommitted = false;
};

struct WHFRAMEWORK_API FVoxelWorldSaveCapture
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	FGuid TransactionId;
	uint64 WorldEpoch = 0;
	FVoxelWorldManifest Manifest;
	TSharedPtr<const FString, ESPMode::ThreadSafe> SourceDirectory;
	TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> SourceLease;
	TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe> SourceDirectoryRoot;
	TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;
	FVoxelCommittedJournalCapture Journal;
	// Worker写完后发布；GT仅在异步保存任务完成回执之后读取。
	TSharedPtr<FVoxelWorldSaveCompletion, ESPMode::ThreadSafe> Completion;
	TSharedPtr<const FVoxelSceneSaveCapture, ESPMode::ThreadSafe> SceneCapture;
	TSharedPtr<const FVoxelSceneSaveCapture, ESPMode::ThreadSafe> RetiredSceneAttempt;
	TSharedPtr<const FVoxelSceneSaveCapture, ESPMode::ThreadSafe> RetiredSceneReceipt;
};

class WHFRAMEWORK_API FVoxelWorldSaveAdapter
{
public:
	bool Capture(FVoxelWorldRuntime& InRuntime, const FVoxelWorldManifest& InManifest,
		TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
		const FVoxelRegionStore& InStore, FString& OutError);
	const FVoxelWorldSaveCapture* GetCapture() const;
	bool AttachSceneCapture(TSharedPtr<const FVoxelSceneSaveCapture, ESPMode::ThreadSafe> InCapture);
	static bool WriteCapture(const FVoxelWorldSaveCapture& InCapture,
		const FString& InTemporaryGenerationDirectory, FString& OutError);
	bool Complete(FVoxelWorldRuntime& InRuntime, FVoxelRegionStore& InStore,
		bool bInSuccess, const FString& InCommittedDirectory,
		TSharedPtr<const FVoxelSceneSaveCapture, ESPMode::ThreadSafe> InRetiredSceneAttempt = {},
		TSharedPtr<const FVoxelSceneSaveCapture, ESPMode::ThreadSafe> InRetiredSceneReceipt = {});
	bool IsBusy() const;
	bool CanCapture() const;
	bool IsCaptureCapacityBlocked() const { return bCaptureCapacityBlocked; }
	const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& GetRetirementResourceLease() const { return RetireTaskLease; }

private:
	friend class FVoxelJournalFixedCutSaveTest;
	friend class FVoxelJournalSaveCapacityRecoveryTest;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetireTaskLease;
	TOptional<FVoxelWorldSaveCapture> Active;
	TOptional<FVoxelWorldSaveCapture> Pending;
	UE::Tasks::FTask RetireCaptureTask;
	uint64 CaptureCapacityVersion = 0;
	bool bCaptureCapacityBlocked = false;
	bool bCaptureRootsFrozen = false;
};
