// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DebugModuleBPLibrary.generated.h"

class ADebugModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UDebugModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "DebugModuleBPLibrary")
	static void EnsureCrash(const FString& Message, bool bNoCrash = false);
	
	UFUNCTION(BlueprintCallable, Category = "DebugModuleBPLibrary")
	static void EnsureEditorCrash(const FString& Message, bool bNoCrash = false);

	UFUNCTION(BlueprintCallable, Category = "DebugModuleBPLibrary")
	static void DebugMessage(const FString& Message, FColor DisplayColor = FColor::Cyan, float Duration = 1.5f, bool bNewerOnTop = true);
};
