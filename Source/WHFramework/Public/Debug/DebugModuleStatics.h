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
	EDC_Default UMETA(DisplayName = "Default"),
	EDC_Editor UMETA(DisplayName = "Editor"),
	EDC_Controller UMETA(DisplayName = "Controller"),
	EDC_Achievement UMETA(DisplayName = "Achievement"),
	EDC_Ability UMETA(DisplayName = "Ability"),
	EDC_Asset UMETA(DisplayName = "Asset"),
	EDC_Audio UMETA(DisplayName = "Audio"),
	EDC_Camera UMETA(DisplayName = "Camera"),
	EDC_Character UMETA(DisplayName = "Character"),
	EDC_Event UMETA(DisplayName = "Event"),
	EDC_FSM UMETA(DisplayName = "FSM"),
	EDC_Input UMETA(DisplayName = "Input"),
	EDC_Animation UMETA(DisplayName = "Animation"),
	EDC_Media UMETA(DisplayName = "Media"),
	EDC_Network UMETA(DisplayName = "Network"),
	EDC_ObjectPool UMETA(DisplayName = "ObjectPool"),
	EDC_Parameter UMETA(DisplayName = "Parameter"),
	EDC_Pawn UMETA(DisplayName = "Pawn"),
	EDC_Procedure UMETA(DisplayName = "Procedure"),
	EDC_ReferencePool UMETA(DisplayName = "ReferencePool"),
	EDC_SaveGame UMETA(DisplayName = "SaveGame"),
	EDC_Scene UMETA(DisplayName = "Scene"),
	EDC_Step UMETA(DisplayName = "Step"),
	EDC_Task UMETA(DisplayName = "Task"),
	EDC_Voxel UMETA(DisplayName = "Voxel"),
	EDC_WebRequest UMETA(DisplayName = "WebRequest"),
	EDC_Widget UMETA(DisplayName = "Widget"),
	EDC_Custom1 UMETA(DisplayName = "Custom1"),
	EDC_Custom2 UMETA(DisplayName = "Custom2"),
	EDC_Custom3 UMETA(DisplayName = "Custom3"),
	EDC_Custom4 UMETA(DisplayName = "Custom4"),
	EDC_Custom5 UMETA(DisplayName = "Custom5"),
	EDC_Custom6 UMETA(DisplayName = "Custom6"),
	EDC_Custom7 UMETA(DisplayName = "Custom7"),
	EDC_Custom8 UMETA(DisplayName = "Custom8"),
	EDC_Custom9 UMETA(DisplayName = "Custom9"),
	EDC_Custom10 UMETA(DisplayName = "Custom10")
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
