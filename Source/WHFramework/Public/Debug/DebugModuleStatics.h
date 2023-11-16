// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "DebugModuleStatics.generated.h"

UENUM(BlueprintType)
enum EDebugMode : int
{
	EDM_All,
	EDM_Screen,
	EDM_Console
};

UENUM(BlueprintType)
enum EDebugCategory : int
{
	EDC_Default,
	EDC_Editor,
	EDC_Controller,
	EDC_Achievement,
	EDC_Ability,
	EDC_Asset,
	EDC_Audio,
	EDC_Camera,
	EDC_Character,
	EDC_Event,
	EDC_FSM,
	EDC_Input,
	EDC_LatentAction,
	EDC_Media,
	EDC_Network,
	EDC_ObjectPool,
	EDC_Parameter,
	EDC_Procedure,
	EDC_ReferencePool,
	EDC_SaveGame,
	EDC_Scene,
	EDC_Step,
	EDC_Task,
	EDC_Voxel,
	EDC_WebRequest,
	EDC_Widget
};

UENUM(BlueprintType)
enum EDebugVerbosity : int
{
	EDV_Log,
	EDV_Warning,
	EDV_Error
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FDebugCategoryState
{
	GENERATED_BODY()

public:
	FORCEINLINE FDebugCategoryState()
	{
		bEnableScreenLog = false;
		bEnableConsoleLog = false;
	}

	FORCEINLINE FDebugCategoryState(bool bInEnableScreenLog, bool bInEnableConsoleLog)
	{
		bEnableScreenLog = bInEnableScreenLog;
		bEnableConsoleLog = bInEnableConsoleLog;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnableScreenLog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnableConsoleLog;
};

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UDebugModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "DebugModuleStatics")
	static bool EnsureEditor(bool InExpression);

	UFUNCTION(BlueprintCallable, Category = "DebugModuleStatics")
	static bool EnsureEditorMsgf(bool InExpression, const FString& InMessage, EDebugCategory InCategory = EDC_Default, EDebugVerbosity InVerbosity = EDV_Log);

	UFUNCTION(BlueprintCallable, Category = "DebugModuleStatics")
	static void LogMessage(const FString& InMessage, EDebugCategory InCategory = EDC_Default, EDebugVerbosity InVerbosity = EDV_Log);

	UFUNCTION(BlueprintCallable, Category = "DebugModuleStatics")
	static void DebugMessage(const FString& InMessage, EDebugMode InMode = EDM_Screen, EDebugCategory InCategory = EDC_Default, EDebugVerbosity InVerbosity = EDV_Log, const FLinearColor InDisplayColor = FLinearColor(0,255,255), float InDuration = 1.5f, int InKey = -1, bool bNewerOnTop = true);

	UFUNCTION(BlueprintPure, Category = "DebugModuleStatics")
	static FDebugCategoryState GetDebugCategoryState(EDebugCategory InCategory);

	UFUNCTION(BlueprintCallable, Category = "DebugModuleStatics")
	static void SetDebugCategoryState(EDebugCategory InCategory, FDebugCategoryState InState);
};
