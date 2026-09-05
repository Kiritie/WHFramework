// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "SaveGame/SaveGameModuleTypes.h"
#include "GameplayTagContainer.h"
#include "TaskModuleTypes.generated.h"

class UTaskBase;
class UTaskAsset;

UENUM(BlueprintType)
enum class ETaskState : uint8
{
	None,
	Entered,
	Executing,
	Completed,
	Leaved
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaskStateChanged, ETaskState, InTaskState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTaskAssetsChanged);

UENUM(BlueprintType)
enum class ETaskExecuteType : uint8
{
	None,
	Automatic,
	Procedure
};

UENUM(BlueprintType)
enum class ETaskEnterType : uint8
{
	None,
	Automatic,
	Procedure
};

UENUM(BlueprintType)
enum class ETaskLeaveType : uint8
{
	None,
	Automatic,
	Procedure
};

UENUM(BlueprintType)
enum class ETaskCompleteType : uint8
{
	None,
	Skip,
	Automatic,
	Procedure
};

UENUM(BlueprintType)
enum class ETaskExecuteResult : uint8
{
	None,
	Succeed,
	Failed,
	Skipped
};

UENUM(BlueprintType)
enum class ETaskGuideType : uint8
{
	None,
	TimerOnce,
	TimerLoop
};

UENUM(BlueprintType)
enum class ETaskTaskState : uint8
{
	None,
	Preparing,
	Executing,
	Completed
};

UENUM(BlueprintType)
enum class ETaskStage : uint8
{
	Locked,
	Available,
	Active,
	ReadyToTurnIn,
	Finished,
	Failed
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FTaskReference
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UTaskAsset> Asset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString TaskGUID;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FTaskObjective
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ObjectiveID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag TargetTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FPrimaryAssetId TargetAssetID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bOptional = false;
};

USTRUCT()
struct WHFRAMEWORK_API FTaskRuntimeSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	FSaveData Archive;

	UPROPERTY()
	float ExecuteRemaining = -1.f;

	UPROPERTY()
	float CompleteRemaining = -1.f;

	UPROPERTY()
	float LeaveRemaining = -1.f;

	UPROPERTY()
	float GuideRemaining = -1.f;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FTaskModuleSaveData : public FSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TSoftObjectPtr<UTaskAsset>> AssetPaths;

	UPROPERTY()
	FString CurrentTaskAssetPath;

	UPROPERTY()
	FString CurrentTaskGUID;

	UPROPERTY()
	TMap<FString, FTaskRuntimeSaveData> TaskRecords;

	virtual void MakeSaved() override
	{
		Super::MakeSaved();
		for (auto& Iter : TaskRecords)
		{
			Iter.Value.Archive.MakeSaved();
		}
	}
};
