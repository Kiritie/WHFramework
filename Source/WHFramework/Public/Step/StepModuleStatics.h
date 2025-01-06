// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "StepModuleTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "StepModuleStatics.generated.h"

class UStepAsset;
class UStepBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UStepModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "ProcedureModuleStatics")
	static TArray<UStepAsset*> GetStepAssets();

	UFUNCTION(BlueprintPure, Category = "StepModuleStatics")
	static UStepAsset* GetCurrentStepAsset();

	UFUNCTION(BlueprintPure, Category = "StepModuleStatics")
	static UStepBase* GetCurrentStep();

	UFUNCTION(BlueprintPure, Category = "StepModuleStatics")
	static UStepBase* GetCurrentRootStep();

	UFUNCTION(BlueprintPure, Category = "StepModuleStatics")
	static UStepBase* GetStepByGUID(const FString& InStepGUID);

public:
	UFUNCTION(BlueprintPure, Category = "StepModuleStatics")
	static UStepAsset* GetStepAsset(UStepAsset* InAsset);
	
	UFUNCTION(BlueprintCallable, Category = "StepModuleStatics")
	static void AddStepAsset(UStepAsset* InAsset);
	
	UFUNCTION(BlueprintCallable, Category = "StepModuleStatics")
	static void RemoveStepAsset(UStepAsset* InAsset);
	
	UFUNCTION(BlueprintCallable, Category = "StepModuleStatics")
	static void SwitchStepAsset(UStepAsset* InAsset);

	UFUNCTION(BlueprintCallable, Category = "StepModuleStatics")
	static void StartStep(int32 InRootStepIndex = -1, bool bSkipSteps = false);

	UFUNCTION(BlueprintCallable, Category = "StepModuleStatics")
	static void EndStep(bool bRestoreSteps = false);

	UFUNCTION(BlueprintCallable, Category = "StepModuleStatics")
	static void RestoreStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable, Category = "StepModuleStatics")
	static void EnterStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable, Category = "StepModuleStatics")
	static void RefreshStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable, Category = "StepModuleStatics")
	static void GuideStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable, Category = "StepModuleStatics")
	static void ExecuteStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable, Category = "StepModuleStatics")
	static void CompleteStep(UStepBase* InStep, EStepExecuteResult InStepExecuteResult = EStepExecuteResult::Succeed);

	UFUNCTION(BlueprintCallable, Category = "StepModuleStatics")
	static void LeaveStep(UStepBase* InStep);
};
