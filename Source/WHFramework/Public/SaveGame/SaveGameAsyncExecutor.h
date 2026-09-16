#pragma once
#include "CoreMinimal.h"
#include "Tasks/Task.h"
#include <atomic>
#include "SaveGame/SaveGameModuleTypes.h"
#include "Voxel/VoxelModule.h"
struct WHFRAMEWORK_API FSaveModuleBytes
{
	FName ModuleName;
	int32 Version = 0;
	TArray<uint8> Bytes;
};
struct WHFRAMEWORK_API FSaveGenerationPlan
{
	int32 UserIndex = 0;
	FGuid SaveId;
	FSaveManifest Manifest;
	FName CurrentMap;
	TArray<FSaveModuleBytes> Modules;
	TOptional<FVoxelModuleSaveCapture> Voxel;
};
class WHFRAMEWORK_API FSaveGameAsyncExecutor
{
public:
	~FSaveGameAsyncExecutor();
	bool Start(FSaveGenerationPlan&& Plan);
	bool Poll(FSaveOperationResult& Out);
	FSaveOperationResult Finish();
	bool IsRunning() const
	{
		return State.IsValid();
	}
	void RequestCancel();

private:
	struct FState
	{
		std::atomic_bool Cancel{false};
		FSaveOperationResult Result = FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save has not finished")));
	};
	static FSaveOperationResult Execute(const FSaveGenerationPlan& Plan, const std::atomic_bool& Cancel);
	TSharedPtr<FState, ESPMode::ThreadSafe> State;
	UE::Tasks::FTask Task;
};
