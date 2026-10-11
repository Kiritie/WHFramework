#pragma once
#include "CoreMinimal.h"
#include "Tasks/Task.h"
#include <atomic>
#include "SaveGame/SaveGameModuleTypes.h"
#include "SaveGame/SaveGameExternalStore.h"
#include "Voxel/VoxelModule.h"
struct WHFRAMEWORK_API FSaveModuleBytes
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FName ModuleName;
	int32 Version = 0;
	TArray<uint8> Bytes;
};
struct WHFRAMEWORK_API FSaveGenerationPlan
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	int32 UserIndex = 0;
	FString StorageRoot;
	FGuid SaveId;
	FGuid TransactionId;
	FGuid OperationId;
	TSharedPtr<FSaveWorldExternalOperation, ESPMode::ThreadSafe> ExternalOperation;
	FSaveManifest Manifest;
	FName CurrentMap;
	TArray<FSaveModuleBytes> Modules;
	TOptional<FVoxelModuleSaveCapture> Voxel;
	bool bDeleteCreatedSlotOnFailure = false;
	bool bCreateSlotAtWrite = false;
	bool bResolveManifestOnWorker = false;
	bool bWriteLastActiveSave = false;
};
class WHFRAMEWORK_API FSaveGameAsyncExecutor
{
public:
	~FSaveGameAsyncExecutor();
	bool Start(FSaveGenerationPlan&& Plan, FString* OutError = nullptr);
	bool Poll(FSaveOperationResult& Out, FVoxelTerrainFrameContext* InFrame = nullptr, bool bForceNativeControl = false);
	bool IsRunning() const
	{
		return State.IsValid();
	}
	void RequestCancel();
	bool IsWaitingForCapacity() const;
	bool IsWaitingForSlot() const;

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FSaveWorldBranchExecutorTest;
#endif
	struct FState
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
		FVoxelGenerationConsumerKey Consumer;
		FSaveGenerationPlan Plan;
		std::atomic_bool Cancel{false};
		FSaveOperationResult Result;
		uint64 AttemptReleaseVersion = 0;
		uint64 AttemptWriterReleaseVersion = 0;
	};
	struct FWorker;
	struct FReclaimer;
	static FSaveOperationResult Execute(const FSaveGenerationPlan& Plan, const std::atomic_bool& Cancel,
		class FSaveGameStorage& Storage);
	TSharedPtr<FState, ESPMode::ThreadSafe> State;
	UE::Tasks::FTask Task;
	double RetryCostMilliseconds = 0.001;
	double CompletionCostMilliseconds = 0.001;
};

