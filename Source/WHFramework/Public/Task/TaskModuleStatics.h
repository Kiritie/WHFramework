// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "TaskModuleTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "TaskModuleStatics.generated.h"

class UTaskBase;
class UTaskAsset;
class AActor;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UTaskModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "TaskModuleStatics")
	static TArray<UTaskAsset*> GetTaskAssets();

	UFUNCTION(BlueprintPure, Category = "TaskModuleStatics")
	static UTaskBase* GetCurrentTask();
	
	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void SetCurrentTask(UTaskBase* InTask);

	UFUNCTION(BlueprintPure, Category = "TaskModuleStatics")
	static bool IsExistTaskByGUID(const FString& InTaskGUID);

	UFUNCTION(BlueprintPure, Category = "TaskModuleStatics")
	static UTaskBase* GetTaskByGUID(const FString& InTaskGUID);

public:
	UFUNCTION(BlueprintPure, Category = "TaskModuleStatics")
	static UTaskAsset* GetTaskAsset(UTaskAsset* InAsset);
	
	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static UTaskAsset* AddTaskAsset(UTaskAsset* InAsset);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static UTaskAsset* CreateTaskAsset(UTaskAsset* InAsset, FGuid InAgentID);
	
	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void RemoveTaskAsset(UTaskAsset* InAsset);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void RestoreTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void RestoreTaskByGUID(const FString& InTaskGUID);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void EnterTask(UTaskBase* InTask, bool bSetAsCurrent = false);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void EnterTaskByGUID(const FString& InTaskGUID, bool bSetAsCurrent = false);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void RefreshTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void RefreshTaskByGUID(const FString& InTaskGUID);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void GuideTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void GuideTaskByGUID(const FString& InTaskGUID);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void ExecuteTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void ExecuteTaskByGUID(const FString& InTaskGUID);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void CompleteTask(UTaskBase* InTask, ETaskExecuteResult InTaskExecuteResult = ETaskExecuteResult::Succeed);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void CompleteTaskByGUID(const FString& InTaskGUID, ETaskExecuteResult InTaskExecuteResult = ETaskExecuteResult::Succeed);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void LeaveTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void LeaveTaskByGUID(const FString& InTaskGUID);

	UFUNCTION(BlueprintPure, Category = "TaskModuleStatics")
	static UTaskBase* ResolveTask(const FTaskReference& InReference);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static bool TurnInTask(UTaskBase* InTask, AActor* InTarget);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void ReportTaskEvent(FGameplayTag InEventTag, FGameplayTag InTargetTag, int32 InCount = 1, FPrimaryAssetId InTargetAssetID = FPrimaryAssetId(), FName InTargetName = NAME_None);
};
