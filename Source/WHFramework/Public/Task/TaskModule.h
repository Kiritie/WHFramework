// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "TaskModuleTypes.h"
#include "Base/TaskBase.h"
#include "Main/Base/ModuleBase.h"

#include "TaskModule.generated.h"

class UTaskBase;

/**
 * 
 */

UCLASS()
class WHFRAMEWORK_API ATaskModule : public AModuleBase, public ISaveDataInterface
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
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
	#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation(EPhase InPhase) override;

protected:
	virtual void Serialize(FArchive& Ar) override;

	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData(bool bRefresh) override;

	virtual bool HasArchive() const override { return true; }

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
	/// Task Stats
protected:
	/// 初始任务 
	UPROPERTY(VisibleAnywhere, Category = "TaskModule|Task Stats")
	UTaskBase* FirstTask;
	/// 当前任务 
	UPROPERTY(VisibleAnywhere, Category = "TaskModule|Task Stats")
	UTaskBase* CurrentTask;
	/// 根任务
	UPROPERTY(VisibleAnywhere, Category = "TaskModule|Task Stats")
	TArray<UTaskBase*> RootTasks;
	/// 任务Map
	UPROPERTY(VisibleAnywhere, Category = "TaskModule|Task Stats")
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
	* 获取当前根任务
	*/
	UFUNCTION(BlueprintPure)
	UTaskBase* GetCurrentRootTask() const;
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
	/**
	* 通过GUID获取任务
	*/
	UFUNCTION(BlueprintPure)
	UTaskBase* GetTaskByGUID(const FString& InTaskGUID) const { return *TaskMap.Find(InTaskGUID); }

	//////////////////////////////////////////////////////////////////////////
	/// Editor
public:
	#if WITH_EDITOR
	void GenerateListItem(TArray<TSharedPtr<struct FTaskListItem>>& OutTaskListItems);

	void UpdateListItem(TArray<TSharedPtr<struct FTaskListItem>>& OutTaskListItems);
	#endif
};
