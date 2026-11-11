// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Task/TaskExecutionTypes.h"
#include "Task/TaskReferenceTypes.h"
#include "Task/TaskObjectiveTypes.h"
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
	static TArray<UTaskAsset*> GetTaskAssets(UWorld* InWorld, FGuid InPlayerId);

	UFUNCTION(BlueprintPure, Category = "TaskModuleStatics")
	static UTaskBase* GetCurrentTask(UWorld* InWorld, FGuid InPlayerId);
	
	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void SetCurrentTask(UWorld* InWorld, FGuid InPlayerId, UTaskBase* InTask);

	UFUNCTION(BlueprintPure, Category = "TaskModuleStatics")
	static bool IsExistTaskByGUID(UWorld* InWorld, const FString& InTaskGUID);

	UFUNCTION(BlueprintPure, Category = "TaskModuleStatics")
	static UTaskBase* GetTaskByGUID(UWorld* InWorld, const FString& InTaskGUID);

public:
	UFUNCTION(BlueprintPure, Category = "TaskModuleStatics")
	static UTaskAsset* GetTaskAsset(UWorld* InWorld, UTaskAsset* InAsset);
	
	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static UTaskAsset* AddTaskAsset(UWorld* InWorld, UTaskAsset* InAsset);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static UTaskAsset* CreateTaskAsset(UWorld* InWorld, UTaskAsset* InAsset, FGuid InAgentID);
	
	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void RemoveTaskAsset(UWorld* InWorld, UTaskAsset* InAsset);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void RestoreTask(UWorld* InWorld, UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void RestoreTaskByGUID(UWorld* InWorld, const FString& InTaskGUID);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void EnterTask(UWorld* InWorld, UTaskBase* InTask, bool bSetAsCurrent = false);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void EnterTaskByGUID(UWorld* InWorld, const FString& InTaskGUID, bool bSetAsCurrent = false);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void RefreshTask(UWorld* InWorld, UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void RefreshTaskByGUID(UWorld* InWorld, const FString& InTaskGUID);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void GuideTask(UWorld* InWorld, UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void GuideTaskByGUID(UWorld* InWorld, const FString& InTaskGUID);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void ExecuteTask(UWorld* InWorld, UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void ExecuteTaskByGUID(UWorld* InWorld, const FString& InTaskGUID);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void CompleteTask(UWorld* InWorld, UTaskBase* InTask, ETaskExecuteResult InTaskExecuteResult = ETaskExecuteResult::Succeed);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void CompleteTaskByGUID(UWorld* InWorld, const FString& InTaskGUID, ETaskExecuteResult InTaskExecuteResult = ETaskExecuteResult::Succeed);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void LeaveTask(UWorld* InWorld, UTaskBase* InTask);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void LeaveTaskByGUID(UWorld* InWorld, const FString& InTaskGUID);

	UFUNCTION(BlueprintPure, Category = "TaskModuleStatics")
	static UTaskBase* ResolveTask(UWorld* InWorld, const FTaskReference& InReference);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static bool TurnInTask(UWorld* InWorld, UTaskBase* InTask, AActor* InTarget);

	UFUNCTION(BlueprintCallable, Category = "TaskModuleStatics")
	static void ReportTaskEvent(UWorld* InWorld, FGameplayTag InEventTag, FGameplayTag InTargetTag, int32 InCount = 1, FPrimaryAssetId InTargetAssetID = FPrimaryAssetId(), FName InTargetName = NAME_None, FGuid InOwnerActorID = FGuid(), ETaskEventScope InScope = ETaskEventScope::Personal);
};
