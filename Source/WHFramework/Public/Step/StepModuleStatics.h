// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "StepModuleTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "StepModuleStatics.generated.h"

class UStepAsset;
class UStepBase;
class UStepModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UStepModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "StepModule", meta = (WorldContext = "WorldContextObject"))
	static TArray<UStepAsset*> GetStepAssets(const UObject* WorldContextObject = nullptr);

	UFUNCTION(BlueprintPure, Category = "StepModule", meta = (WorldContext = "WorldContextObject"))
	static UStepAsset* GetCurrentStepAsset(const UObject* WorldContextObject = nullptr);

	UFUNCTION(BlueprintPure, Category = "StepModule", meta = (WorldContext = "WorldContextObject"))
	static UStepBase* GetCurrentStep(const UObject* WorldContextObject = nullptr);

	UFUNCTION(BlueprintPure, Category = "StepModule", meta = (WorldContext = "WorldContextObject"))
	static UStepBase* GetCurrentRootStep(const UObject* WorldContextObject = nullptr);

	UFUNCTION(BlueprintPure, Category = "StepModule", meta = (WorldContext = "WorldContextObject"))
	static UStepBase* GetStepByGUID(const FString& InStepGUID, const UObject* WorldContextObject = nullptr);

public:
	UFUNCTION(BlueprintPure, Category = "StepModule", meta = (WorldContext = "WorldContextObject"))
	static UStepAsset* GetStepAsset(UStepAsset* InAsset, const UObject* WorldContextObject = nullptr);
	
	UFUNCTION(BlueprintCallable, Category = "StepModule", meta = (WorldContext = "WorldContextObject"))
	static void AddStepAsset(UStepAsset* InAsset, const UObject* WorldContextObject = nullptr);
	
	UFUNCTION(BlueprintCallable, Category = "StepModule", meta = (WorldContext = "WorldContextObject"))
	static void RemoveStepAsset(UStepAsset* InAsset, const UObject* WorldContextObject = nullptr);
	
	UFUNCTION(BlueprintCallable, Category = "StepModule", meta = (WorldContext = "WorldContextObject"))
	static void SwitchStepAsset(UStepAsset* InAsset, const UObject* WorldContextObject = nullptr);

	UFUNCTION(BlueprintCallable, Category = "StepModule", meta = (WorldContext = "WorldContextObject"))
	static void StartStep(int32 InRootStepIndex = -1, bool bSkipSteps = false, const UObject* WorldContextObject = nullptr);

	UFUNCTION(BlueprintCallable, Category = "StepModule", meta = (WorldContext = "WorldContextObject"))
	static void EndStep(bool bRestoreSteps = false, const UObject* WorldContextObject = nullptr);

	UFUNCTION(BlueprintCallable, Category = "StepModule")
	static void RestoreStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable, Category = "StepModule")
	static void EnterStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable, Category = "StepModule")
	static void RefreshStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable, Category = "StepModule")
	static void GuideStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable, Category = "StepModule")
	static void ExecuteStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable, Category = "StepModule")
	static void CompleteStep(UStepBase* InStep, EStepExecuteResult InStepExecuteResult = EStepExecuteResult::Succeed);

	UFUNCTION(BlueprintCallable, Category = "StepModule")
	static void LeaveStep(UStepBase* InStep);
private:
	static UStepModule* ResolveModule(const UObject* WorldContextObject);
};
