// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Asset/Base/AssetBase.h"
#include "TaskAsset.generated.h"

class UTaskBase;
/**
 * Imported spreadsheet table.
 */
UCLASS(BlueprintType, Meta = (LoadBehavior = "LazyOnDemand"))
class WHFRAMEWORK_API UTaskAsset : public UAssetBase
{
	GENERATED_UCLASS_BODY()

public:
	virtual void Initialize(UAssetBase* InSource) override;

	//////////////////////////////////////////////////////////////////////////
	/// Task Stats
public:
	/// 初始任务 
	UPROPERTY(VisibleAnywhere)
	UTaskBase* FirstTask;
	/// 根任务
	UPROPERTY(VisibleAnywhere)
	TArray<UTaskBase*> RootTasks;
	/// 任务Map
	UPROPERTY(Transient)
	TMap<FString, UTaskBase*> TaskMap;

public:
	/**
	* 获取初始任务
	*/
	UFUNCTION(BlueprintPure)
	UTaskBase* GetFirstTask() const { return FirstTask; }
	/**
	* 设置初始任务
	*/
	UFUNCTION(BlueprintCallable)
	void SetFirstTask(UTaskBase* InFirstTask) { this->FirstTask = InFirstTask; }
	/**
	* 获取根任务列表
	*/
	UFUNCTION(BlueprintPure)
	TArray<UTaskBase*>& GetRootTasks() { return RootTasks; }
	/**
	* 获取任务Map
	*/
	UFUNCTION(BlueprintPure)
	TMap<FString, UTaskBase*>& GetTaskMap() { return TaskMap; }

	//////////////////////////////////////////////////////////////////////////
	/// Editor
public:
#if WITH_EDITOR
	void GenerateTaskListItem(TArray<TSharedPtr<struct FTaskListItem>>& OutTaskListItems, const FString& InFilterText = TEXT(""));

	void UpdateTaskListItem(TArray<TSharedPtr<struct FTaskListItem>>& OutTaskListItems);

	bool CanAddTask(TSubclassOf<UTaskBase> InTaskClass);

	void ClearAllTask();
#endif
};
