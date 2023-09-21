// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "ProcedureModuleTypes.generated.h"

UENUM(BlueprintType)
enum class EProcedureState : uint8
{
	None,
	Entered,
	Leaved
};

UENUM(BlueprintType)
enum class EProcedureGuideType : uint8
{
	None,
	TimerOnce,
	TimerLoop
};

UENUM(BlueprintType)
enum class EProcedureCameraViewMode : uint8
{
	None,
	Instant,
	Smooth,
	Duration
};

UENUM(BlueprintType)
enum class EProcedureCameraViewSpace : uint8
{
	Local,
	World
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FProcedureListItemStates
{
	GENERATED_BODY()

public:
	FORCEINLINE FProcedureListItemStates()
	{
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProcedureStateChanged, EProcedureState, InProcedureState);
