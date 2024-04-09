// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Debug/DebugTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DebugModuleStatics.generated.h"

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
