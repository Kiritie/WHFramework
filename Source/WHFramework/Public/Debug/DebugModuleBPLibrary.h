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
	static void DebugMessage(const FString& Message, const FColor& DisplayColor = FColor::Cyan, float Duration = 1.5f, EDebugMode DebugMode = EDebugMode::Screen, int32 Key = -1, bool bNewerOnTop = true);
};
