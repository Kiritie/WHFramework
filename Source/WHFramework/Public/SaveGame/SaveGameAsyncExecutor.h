#pragma once
#include "SaveGame/SaveOperationTypes.h"
#include "SaveGame/SaveManifestTypes.h"
#include "CoreMinimal.h"
#include "Tasks/Task.h"
#include "HAL/Event.h"
#include <atomic>
#include "SaveGame/SaveGameModuleTypes.h"
#include "SaveGame/SaveGameExternalStore.h"
#include "Voxel/VoxelModule.h"
class FSaveGameLocalContext;
struct WHFRAMEWORK_API FSaveModuleBytes
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FName ModuleName;
	int32 Version = 0;
	TArray<uint8> Bytes;
};
struct WHFRAMEWORK_API FSaveGenerationPlan
{
	TSharedPtr<const FSaveGameLocalContext, ESPMode::ThreadSafe> LocalSaveContext;
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
	bool WaitForNativeExit(uint32 TimeoutMilliseconds) const;
	bool IsWaitingForCapacity() const;
	bool IsWaitingForSlot() const;
	/** Call after world hosts have stopped, before unloading WHFramework code. */
	static void DrainRetiredTasks();
	static void TrackNativeRetirement(const UE::Tasks::FTask& InTask, TSharedPtr<void, ESPMode::ThreadSafe> InLifetime = {});

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
	struct FWorkerCompletion;
	struct FReclaimer;
	static FSaveOperationResult Execute(const FSaveGenerationPlan& Plan, const std::atomic_bool& Cancel,
		class FSaveGameStorage& Storage);
	void LaunchWorker();
	TSharedPtr<FState, ESPMode::ThreadSafe> State;
	UE::Tasks::FTask Task;
	mutable UE::Tasks::FTask CompletionTask;
	mutable TSharedPtr<FEventRef, ESPMode::ThreadSafe> NativeExit;
	double RetryCostMilliseconds = 0.001;
	double CompletionCostMilliseconds = 0.001;
};

