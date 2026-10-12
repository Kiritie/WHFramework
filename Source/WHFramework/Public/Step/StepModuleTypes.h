#pragma once

#include "CoreMinimal.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "StepModuleTypes.generated.h"

class UStepAsset;

UENUM(BlueprintType)
enum class EStepState : uint8
{
	None,
	Entered,
	Executing,
	Completed,
	Leaved
};

UENUM(BlueprintType)
enum class EStepModuleState : uint8
{
	None,
	Running,
	Paused,
	Ended
};

UENUM(BlueprintType)
enum class EStepExecuteType : uint8
{
	None,
	Automatic,
	Procedure,
	MouseClick
};

UENUM(BlueprintType)
enum class EStepLeaveType : uint8
{
	None,
	Automatic,
	Procedure
};

UENUM(BlueprintType)
enum class EStepCompleteType : uint8
{
	None,
	Skip,
	Automatic,
	Procedure
};

UENUM(BlueprintType)
enum class EStepExecuteResult : uint8
{
	None,
	Succeed,
	Failed,
	Skipped
};

UENUM(BlueprintType)
enum class EStepGuideType : uint8
{
	None,
	TimerOnce,
	TimerLoop
};

UENUM(BlueprintType)
enum class EStepTaskState : uint8
{
	None,
	Preparing,
	Executing,
	Completed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStepStateChanged, EStepState, InStepState);

USTRUCT()
struct WHFRAMEWORK_API FStepGraphLink
{
	GENERATED_BODY()

	UPROPERTY()
	FString ParentGUID;

	UPROPERTY()
	FString ChildGUID;

	bool operator==(const FStepGraphLink& Other) const
	{
		return ParentGUID == Other.ParentGUID && ChildGUID == Other.ChildGUID;
	}
};

USTRUCT()
struct WHFRAMEWORK_API FStepCompiledNode
{
	GENERATED_BODY()

	UPROPERTY()
	FString StepGUID;

	UPROPERTY()
	FString ParentGUID;

	UPROPERTY()
	TArray<FString> ChildrenGUIDs;

	UPROPERTY()
	int32 Index = INDEX_NONE;

	UPROPERTY()
	int32 Hierarchy = INDEX_NONE;

	UPROPERTY()
	FString OrderLabel;
};

USTRUCT()
struct WHFRAMEWORK_API FStepCompiledGraph
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FString> RootGUIDs;

	UPROPERTY()
	TArray<FStepCompiledNode> Nodes;

	UPROPERTY()
	FString Signature;
};

USTRUCT()
struct WHFRAMEWORK_API FStepTaskSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	FName TaskName;

	UPROPERTY()
	EStepTaskState State = EStepTaskState::None;

	UPROPERTY()
	float Remaining = 0.f;
};

USTRUCT()
struct WHFRAMEWORK_API FStepRuntimeSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	FString StepGUID;

	UPROPERTY()
	EStepState State = EStepState::None;

	UPROPERTY()
	EStepExecuteResult Result = EStepExecuteResult::None;

	UPROPERTY()
	FString CurrentChildGUID;

	UPROPERTY()
	FName CurrentTaskName;

	UPROPERTY()
	float AutoExecuteRemaining = -1.f;

	UPROPERTY()
	float AutoCompleteRemaining = -1.f;

	UPROPERTY()
	float AutoLeaveRemaining = -1.f;

	UPROPERTY()
	float GuideRemaining = -1.f;

	UPROPERTY()
	TArray<FStepTaskSaveData> Tasks;

	UPROPERTY()
	TArray<uint8> CustomData;
};

USTRUCT()
struct WHFRAMEWORK_API FStepAssetSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	TSoftObjectPtr<UStepAsset> Asset;

	UPROPERTY()
	FString GraphSignature;

	UPROPERTY()
	TArray<FStepRuntimeSaveData> Steps;
};

USTRUCT()
struct WHFRAMEWORK_API FStepModuleSaveData : public FSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FStepAssetSaveData> Assets;

	UPROPERTY()
	TSoftObjectPtr<UStepAsset> CurrentAsset;

	UPROPERTY()
	FString CurrentStepGUID;

	UPROPERTY()
	FString CurrentRootGUID;

	UPROPERTY()
	EStepModuleState State = EStepModuleState::None;
};

UENUM()
enum class EStepDeferredAction : uint8
{
	None,
	AddAsset,
	RemoveAsset,
	SwitchAsset,
	Start,
	End,
	Restore
};

USTRUCT()
struct WHFRAMEWORK_API FStepDeferredCommand
{
	GENERATED_BODY()

	UPROPERTY()
	EStepDeferredAction Action = EStepDeferredAction::None;

	UPROPERTY()
	UStepAsset* Asset = nullptr;

	UPROPERTY()
	FString StepGUID;

	UPROPERTY()
	int32 RootIndex = INDEX_NONE;

	UPROPERTY()
	bool bOption = false;
};
