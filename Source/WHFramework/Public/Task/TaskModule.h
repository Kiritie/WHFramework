// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "TaskModuleTypes.h"
#include "Base/TaskBase.h"
#include "Main/Base/ModuleBase.h"

#include "TaskModule.generated.h"

class UTaskBase;
class URootTaskBase;

/**
 * 
 */

UCLASS()
class WHFRAMEWORK_API ATaskModule : public AModuleBase
{
	GENERATED_BODY()
		
	GENERATED_MODULE(ATaskModule)

public:
	// ParamSets default values for this actor's properties
	ATaskModule();

	~ATaskModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
	#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
	#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// TaskModule
protected:
	/// 自动进入初始任务
	UPROPERTY(EditAnywhere, Category = "TaskModule")
	bool bAutoEnterFirst;

public:
	UFUNCTION(BlueprintCallable)
	void RestoreTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable)
	void EnterTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable)
	void RefreshTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable)
	void GuideTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable)
	void ExecuteTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable)
	void CompleteTask(UTaskBase* InTask, ETaskExecuteResult InTaskExecuteResult = ETaskExecuteResult::Succeed);

	UFUNCTION(BlueprintCallable)
	void LeaveTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable)
	void ClearAllTask();

public:
	UFUNCTION(BlueprintPure)
	bool IsAllTaskCompleted();

	//////////////////////////////////////////////////////////////////////////
	/// Editor
public:
	#if WITH_EDITOR
	void GenerateListItem(TArray<TSharedPtr<struct FTaskListItem>>& OutTaskListItems);

	void UpdateListItem(TArray<TSharedPtr<struct FTaskListItem>>& OutTaskListItems);

	void SetRootTaskItem(int32 InIndex, URootTaskBase* InRootTask);
	#endif

	//////////////////////////////////////////////////////////////////////////
	/// Root Task
protected:
	/// 根任务
	UPROPERTY(VisibleAnywhere, Category = "TaskModule|Root Tasks")
	TArray<URootTaskBase*> RootTasks;

public:
	/**
	* 获取当前根任务
	*/
	UFUNCTION(BlueprintPure)
	URootTaskBase* GetCurrentRootTask() const
	{
		if(CurrentTask)
		{
			return CurrentTask->RootTask;
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure)
	TArray<URootTaskBase*>& GetRootTasks() { return RootTasks; }

	//////////////////////////////////////////////////////////////////////////
	/// Task Stats
protected:
	/// 初始任务 
	UPROPERTY(VisibleAnywhere, Category = "TaskModule|Task Stats")
	UTaskBase* FirstTask;
	
	/// 当前任务 
	UPROPERTY(VisibleAnywhere, Transient, Category = "TaskModule|Task Stats")
	UTaskBase* CurrentTask;
	
	/// 任务Map
	UPROPERTY(VisibleAnywhere, Transient, Category = "TaskModule|Task Stats")
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
	* 获取当前任务
	*/
	UFUNCTION(BlueprintPure)
	UTaskBase* GetCurrentTask() const { return CurrentTask; }
	/**
	* 获取任务Map
	*/
	UFUNCTION(BlueprintPure)
	TMap<FString, UTaskBase*>& GetTaskMap() { return TaskMap; }
	/**
	* 通过GUID获取任务
	*/
	UFUNCTION(BlueprintPure)
	UTaskBase* GetTaskByGUID(const FString& InTaskGUID) const { return *TaskMap.Find(InTaskGUID); }
};
