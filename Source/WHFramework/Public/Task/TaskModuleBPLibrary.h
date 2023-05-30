// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
#include "TaskModuleTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "TaskModuleBPLibrary.generated.h"

class URootTaskBase;
class UTaskBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UTaskModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "TaskModuleBPLibrary")
	static UTaskBase* GetCurrentTask();

	UFUNCTION(BlueprintPure, Category = "TaskModuleBPLibrary")
	static URootTaskBase* GetCurrentRootTask();

	UFUNCTION(BlueprintPure, Category = "TaskModuleBPLibrary")
	static UTaskBase* GetTaskByGUID(const FString& InTaskGUID);

public:
	UFUNCTION(BlueprintCallable, Category = "TaskModuleBPLibrary")
	static void RestoreTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleBPLibrary")
	static void EnterTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleBPLibrary")
	static void RefreshTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleBPLibrary")
	static void GuideTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleBPLibrary")
	static void ExecuteTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleBPLibrary")
	static void CompleteTask(UTaskBase* InTask, ETaskExecuteResult InTaskExecuteResult = ETaskExecuteResult::Succeed);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleBPLibrary")
	static void LeaveTask(UTaskBase* InTask);
};
