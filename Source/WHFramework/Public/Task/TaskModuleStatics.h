// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "TaskModuleTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "TaskModuleStatics.generated.h"

class UTaskBase;
class UTaskAsset;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UTaskModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "TaskModuleStatics")
	static UTaskAsset* GetCurrentTaskAsset();

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void SetCurrentTaskAsset(UTaskAsset* InTaskAsset, bool bAutoEnterFirst = false);

	UFUNCTION(BlueprintPure, Category = "TaskModuleStatics")
	static UTaskBase* GetCurrentTask();

	UFUNCTION(BlueprintPure, Category = "TaskModuleStatics")
	static UTaskBase* GetCurrentRootTask();

	UFUNCTION(BlueprintPure, Category = "TaskModuleStatics")
	static UTaskBase* GetTaskByGUID(const FString& InTaskGUID);

public:
	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void RestoreTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void EnterTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void RefreshTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void GuideTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void ExecuteTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void CompleteTask(UTaskBase* InTask, ETaskExecuteResult InTaskExecuteResult = ETaskExecuteResult::Succeed);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void LeaveTask(UTaskBase* InTask);
};
