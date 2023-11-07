// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "DebugModuleBPLibrary.generated.h"

UENUM(BlueprintType)
enum EDebugMode : int
{
	EM_All,
	EM_Screen,
	EM_Console
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
class WHFRAMEWORK_API UDebugModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "DebugModuleBPLibrary")
	static bool Ensure(bool Expression);
	
	UFUNCTION(BlueprintCallable, Category = "DebugModuleBPLibrary")
	static bool EnsureMsgf(bool Expression, const FString& Message, EDebugCategory Category = EDC_Default, EDebugVerbosity Verbosity = EDV_Log);
		
	UFUNCTION(BlueprintCallable, Category = "DebugModuleBPLibrary")
	static bool EnsureEditor(bool Expression);

	UFUNCTION(BlueprintCallable, Category = "DebugModuleBPLibrary")
	static bool EnsureEditorMsgf(bool Expression, const FString& Message, EDebugCategory Category = EDC_Default, EDebugVerbosity Verbosity = EDV_Log);

	UFUNCTION(BlueprintCallable, Category = "DebugModuleBPLibrary")
	static void LogMessage(const FString& Message, EDebugCategory Category = EDC_Default, EDebugVerbosity Verbosity = EDV_Log);

	UFUNCTION(BlueprintCallable, Category = "DebugModuleBPLibrary")
	static void DebugMessage(const FString& Message, EDebugMode Mode = EM_Screen, EDebugCategory Category = EDC_Default, EDebugVerbosity Verbosity = EDV_Log, const FColor& DisplayColor = FColor::Cyan, float Duration = 1.5f, int Key = -1, bool bNewerOnTop = true);

	UFUNCTION(BlueprintPure, Category = "DebugModuleBPLibrary")
	static FDebugCategoryState GetDebugCategoryState(EDebugCategory InCategory);

	UFUNCTION(BlueprintCallable, Category = "DebugModuleBPLibrary")
	static void SetDebugCategoryState(EDebugCategory InCategory, FDebugCategoryState InState);
};
