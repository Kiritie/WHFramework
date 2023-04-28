// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DebugModuleBPLibrary.generated.h"

UENUM(BlueprintType)
enum class EDebugMode : uint8
{
	All,
	Screen,
	Console
};

UENUM(BlueprintType)
enum class EDebugCategory : uint8
{
	Default,
	Editor,
	Controller,
	Ability,
	Asset,
	Audio,
	Camera,
	Character,
	Event,
	FSM,
	Input,
	LatentAction,
	Media,
	Network,
	ObjectPool,
	Parameter,
	Procedure,
	ReferencePool,
	SaveGame,
	Scene,
	Step,
	Task,
	Voxel,
	WebRequest,
	Widget
};

UENUM(BlueprintType)
enum class EDebugVerbosity : uint8
{
	Log,
	Warning,
	Error
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
	static void Ensure(bool Expression = true);
	
	UFUNCTION(BlueprintCallable, Category = "DebugModuleBPLibrary")
	static void EnsureMsgf(const FString& Message, bool Expression = true);
		
	UFUNCTION(BlueprintCallable, Category = "DebugModuleBPLibrary")
	static void EnsureEditor(bool Expression = true);

	UFUNCTION(BlueprintCallable, Category = "DebugModuleBPLibrary")
	static void EnsureEditorMsgf(const FString& Message, bool Expression = true);

	UFUNCTION(BlueprintCallable, Category = "DebugModuleBPLibrary")
	static void LogMessage(const FString& Message, EDebugCategory Category = EDebugCategory::Default, EDebugVerbosity Verbosity = EDebugVerbosity::Log);

	UFUNCTION(BlueprintCallable, Category = "DebugModuleBPLibrary")
	static void DebugMessage(const FString& Message, EDebugMode Mode = EDebugMode::Screen, EDebugCategory Category = EDebugCategory::Default, EDebugVerbosity Verbosity = EDebugVerbosity::Log, const FColor& DisplayColor = FColor::Cyan, float Duration = 1.5f, int32 Key = -1, bool bNewerOnTop = true);

	UFUNCTION(BlueprintPure, Category = "DebugModuleBPLibrary")
	static FDebugCategoryState GetDebugCategoryState(EDebugCategory InCategory);

	UFUNCTION(BlueprintCallable, Category = "DebugModuleBPLibrary")
	static void SetDebugCategoryState(EDebugCategory InCategory, FDebugCategoryState InState);
};
