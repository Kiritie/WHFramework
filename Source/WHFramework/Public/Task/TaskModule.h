// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "TaskModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "TaskModule.generated.h"

class UTaskBase;
class URootTaskBase;

/**
 * 
 */

UENUM(BlueprintType)
enum class ETaskModuleState : uint8
{
	None,
	Running,
	Paused,
	Ended
};

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
	/// 自动开始初始任务
	UPROPERTY(EditAnywhere, Category = "TaskModule")
	bool bAutoStartFirst;
	
	/// 任务模块状态
	UPROPERTY(VisibleAnywhere, Category = "TaskModule")
	ETaskModuleState TaskModuleState;

public:
	/**
	* 获取任务模块状态
	*/
	UFUNCTION(BlueprintPure)
	ETaskModuleState GetTaskModuleState() const { return TaskModuleState; }

public:
	UFUNCTION(BlueprintCallable)
	void StartTask(int32 InRootTaskIndex = -1, bool bSkipTasks = false);

	UFUNCTION(BlueprintCallable)
	void EndTask(bool bRestoreTasks = false);

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
	/// 当前根任务索引
	UPROPERTY(VisibleAnywhere, Category = "TaskModule|Root Tasks")
	int32 CurrentRootTaskIndex;
	/// 根任务
	UPROPERTY(VisibleAnywhere, Category = "TaskModule|Root Tasks")
	TArray<URootTaskBase*> RootTasks;

public:
	UFUNCTION(BlueprintPure)
	int32 GetCurrentRootTaskIndex() const { return CurrentRootTaskIndex; }

	/**
	* 获取当前根任务
	*/
	UFUNCTION(BlueprintPure)
	URootTaskBase* GetCurrentRootTask() const
	{
		if(RootTasks.IsValidIndex(CurrentRootTaskIndex))
		{
			return RootTasks[CurrentRootTaskIndex];
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

	//////////////////////////////////////////////////////////////////////////
	/// Global Options
protected:
	/// 任务执行方式
	UPROPERTY(EditAnywhere, Category = "TaskModule|Global Options")
	ETaskExecuteType GlobalTaskExecuteType;
	/// 任务完成方式
	UPROPERTY(EditAnywhere, Category = "TaskModule|Global Options")
	ETaskCompleteType GlobalTaskCompleteType;
	/// 任务离开方式
	UPROPERTY(EditAnywhere, Category = "TaskModule|Global Options")
	ETaskLeaveType GlobalTaskLeaveType;

public:
	UFUNCTION(BlueprintPure)
	ETaskExecuteType GetGlobalTaskExecuteType() const { return GlobalTaskExecuteType; }

	UFUNCTION(BlueprintCallable)
	void SetGlobalTaskExecuteType(ETaskExecuteType InGlobalTaskExecuteType);

	UFUNCTION(BlueprintPure)
	ETaskCompleteType GetGlobalTaskCompleteType() const { return GlobalTaskCompleteType; }

	UFUNCTION(BlueprintCallable)
	void SetGlobalTaskCompleteType(ETaskCompleteType InGlobalTaskCompleteType);

	UFUNCTION(BlueprintPure)
	ETaskLeaveType GetGlobalTaskLeaveType() const { return GlobalTaskLeaveType; }

	UFUNCTION(BlueprintCallable)
	void SetGlobalTaskLeaveType(ETaskLeaveType InGlobalTaskLeaveType);
};
