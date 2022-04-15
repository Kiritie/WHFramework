// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "UObject/NoExportTypes.h"
#include "ProcedureModuleTypes.generated.h"

UENUM(BlueprintType)
enum class EProcedureType : uint8
{
	Root,
	Default,
	Standalone
};

UENUM(BlueprintType)
enum class EProcedureState : uint8
{
	None,
	Entered,
	Executing,
	Completed,
	Leaved
};

UENUM(BlueprintType)
enum class EProcedureExecuteType : uint8
{
	None,
	Automatic,
	Procedure,
	MouseClick
};

UENUM(BlueprintType)
enum class EProcedureLeaveType : uint8
{
	None,
	Automatic,
	Procedure
};

UENUM(BlueprintType)
enum class EProcedureCompleteType : uint8
{
	None,
	Skip,
	Automatic,
	Procedure
};

UENUM(BlueprintType)
enum class EProcedureExecuteResult : uint8
{
	None,
	Succeed,
	Failed,
	Skipped
};

UENUM(BlueprintType)
enum class EProcedureGuideType : uint8
{
	None,
	TimerOnce,
	TimerLoop
};

UENUM(BlueprintType)
enum class EProcedureTaskState : uint8
{
	None,
	Preparing,
	Executing,
	Completed
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FProcedureListItemStates
{
	GENERATED_BODY()

public:
	UPROPERTY()
	bool bExpanded;

	FORCEINLINE FProcedureListItemStates()
	{
		bExpanded = true;
	}
};
