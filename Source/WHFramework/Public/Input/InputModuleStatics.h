// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "InputModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InputModuleStatics.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UInputModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	//////////////////////////////////////////////////////////////////////////
	// InputShortcuts
public:
	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static FInputKeyShortcut GetKeyShortcutByName(const FName InName);

	//////////////////////////////////////////////////////////////////////////
	// InputStates
public:
	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static int32 GetTouchPressedCount();

	//////////////////////////////////////////////////////////////////////////
	// InputMode
public:
	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static EInputMode GetGlobalInputMode();

	UFUNCTION(BlueprintCallable, Category = "InputModuleStatics")
	static void SetGlobalInputMode(EInputMode InInputMode);

	UFUNCTION(BlueprintCallable, Category = "InputModuleStatics")
	static void UpdateGlobalInputMode();
};