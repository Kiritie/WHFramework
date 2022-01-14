// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "InputModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InputModuleBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UInputModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	//////////////////////////////////////////////////////////////////////////
	// InputMode
public:
	UFUNCTION(BlueprintPure, Category = "InputModuleBPLibrary")
	static EInputMode GetInputMode();

	UFUNCTION(BlueprintCallable, Category = "InputModuleBPLibrary")
	static void SetInputMode(EInputMode InInputMode);
};