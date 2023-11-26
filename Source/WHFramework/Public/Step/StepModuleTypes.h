// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "StepModuleTypes.generated.h"

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

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FStepListItemStates
{
	GENERATED_BODY()

public:
	UPROPERTY()
	bool bExpanded;

	FORCEINLINE FStepListItemStates()
	{
		bExpanded = true;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStepStateChanged, EStepState, InStepState);
