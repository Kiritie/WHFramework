// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "SaveGame/SaveGameModuleTypes.h"
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

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FTaskModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FTaskModuleSaveData()
	{
		Assets = TArray<UTaskAsset*>();
		CurrentAsset = nullptr;
		CurrentTask = nullptr;
		TaskDataMap = TMap<FString, FSaveData>();
	}

public:
	UPROPERTY()
	TArray<UTaskAsset*> Assets;

	UPROPERTY()
	UTaskAsset* CurrentAsset;

	UPROPERTY()
	UTaskBase* CurrentTask;

	UPROPERTY()
	TMap<FString, FSaveData> TaskDataMap;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();

		for(auto& Iter : TaskDataMap)
		{
			Iter.Value.MakeSaved();
		}
	}
};
