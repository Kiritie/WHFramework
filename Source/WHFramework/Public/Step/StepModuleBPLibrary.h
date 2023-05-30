// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
#include "StepModuleTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "StepModuleBPLibrary.generated.h"

class URootStepBase;
class UStepBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UStepModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "StepModuleBPLibrary")
	static UStepBase* GetCurrentStep();

	UFUNCTION(BlueprintPure, Category = "StepModuleBPLibrary")
	static URootStepBase* GetCurrentRootStep();

	UFUNCTION(BlueprintPure, Category = "StepModuleBPLibrary")
	static UStepBase* GetStepByGUID(const FString& InStepGUID);

public:
	UFUNCTION(BlueprintCallable, Category = "StepModuleBPLibrary")
	static void StartStep(int32 InRootStepIndex = -1, bool bSkipSteps = false);

	UFUNCTION(BlueprintCallable, Category = "StepModuleBPLibrary")
	static void EndStep(bool bRestoreSteps = false);

	UFUNCTION(BlueprintCallable, Category = "StepModuleBPLibrary")
	static void RestoreStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable, Category = "StepModuleBPLibrary")
	static void EnterStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable, Category = "StepModuleBPLibrary")
	static void RefreshStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable, Category = "StepModuleBPLibrary")
	static void GuideStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable, Category = "StepModuleBPLibrary")
	static void ExecuteStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable, Category = "StepModuleBPLibrary")
	static void CompleteStep(UStepBase* InStep, EStepExecuteResult InStepExecuteResult = EStepExecuteResult::Succeed);

	UFUNCTION(BlueprintCallable, Category = "StepModuleBPLibrary")
	static void LeaveStep(UStepBase* InStep);
};
