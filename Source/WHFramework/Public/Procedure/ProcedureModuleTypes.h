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
	Initialized,
	Prepared,
	Entered,
	Completed,
	Leaved
};

UENUM(BlueprintType)
enum class EProcedureExecuteType : uint8
{
	None,
	Server,
	Local,
	Inherit
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
	TimerLoop,
	Listener,
};

UENUM(BlueprintType)
enum class EProcedureTaskState : uint8
{
	None,
	Preparing,
	Executing,
	Completed
};
