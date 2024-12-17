// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "TaskModuleTypes.h"
#include "Base/TaskAsset.h"
#include "Base/TaskBase.h"
#include "Main/Base/ModuleBase.h"

#include "TaskModule.generated.h"

class UTaskBase;

/**
 * 
 */

UCLASS()
class WHFRAMEWORK_API UTaskModule : public UModuleBase
{
	GENERATED_BODY()
		
	GENERATED_MODULE(UTaskModule)

public:
	// ParamSets default values for this actor's properties
	UTaskModule();

	~UTaskModule();

	//////////////////////////////////////////////////////////////////////////
	/// ModuleBase
public:
	#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
	#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual void UnloadData(EPhase InPhase) override;

	virtual FSaveData* ToData() override;

public:
	virtual FString GetModuleDebugMessage() override;

	//////////////////////////////////////////////////////////////////////////
	/// TaskModule
public:
	UFUNCTION(BlueprintPure)
	UTaskAsset* GetAsset(UTaskAsset* InAsset) const;
	
	UFUNCTION(BlueprintCallable)
	void AddAsset(UTaskAsset* InAsset);
	
	UFUNCTION(BlueprintCallable)
	void RemoveAsset(UTaskAsset* InAsset);

	UFUNCTION(BlueprintCallable)
	void RestoreTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable)
	void RestoreTaskByGUID(const FString& InTaskGUID);

	UFUNCTION(BlueprintCallable)
	void EnterTask(UTaskBase* InTask, bool bSetAsCurrent = false);

	UFUNCTION(BlueprintCallable)
	void EnterTaskByGUID(const FString& InTaskGUID, bool bSetAsCurrent = false);

	UFUNCTION(BlueprintCallable)
	void RefreshTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable)
	void RefreshTaskByGUID(const FString& InTaskGUID);

	UFUNCTION(BlueprintCallable)
	void GuideTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable)
	void GuideTaskByGUID(const FString& InTaskGUID);

	UFUNCTION(BlueprintCallable)
	void ExecuteTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable)
	void ExecuteTaskByGUID(const FString& InTaskGUID);

	UFUNCTION(BlueprintCallable)
	void CompleteTask(UTaskBase* InTask, ETaskExecuteResult InTaskExecuteResult = ETaskExecuteResult::Succeed);

	UFUNCTION(BlueprintCallable)
	void CompleteTaskByGUID(const FString& InTaskGUID, ETaskExecuteResult InTaskExecuteResult = ETaskExecuteResult::Succeed);

	UFUNCTION(BlueprintCallable)
	void LeaveTask(UTaskBase* InTask);

	UFUNCTION(BlueprintCallable)
	void LeaveTaskByGUID(const FString& InTaskGUID);

public:
	UFUNCTION(BlueprintPure)
	bool IsAllTaskCompleted() const;

	//////////////////////////////////////////////////////////////////////////
	/// Task Stats
protected:
	/// 流程资产列表
	UPROPERTY(VisibleAnywhere, Category = "TaskModule|Task Stats")
	TArray<UTaskAsset*> Assets;
	/// 默认流程资产 
	UPROPERTY(EditAnywhere, Category = "TaskModule|Task Stats")
	TArray<UTaskAsset*> DefaultAssets;
	/// 当前任务 
	UPROPERTY(VisibleAnywhere, Category = "TaskModule|Task Stats")
	UTaskBase* CurrentTask;

public:
	/**
	* 获取资产列表
	*/
	UFUNCTION(BlueprintPure)
	TArray<UTaskAsset*> GetAssets() const { return Assets; }
	/**
	* 获取默认资产列表
	*/
	UFUNCTION(BlueprintPure)
	TArray<UTaskAsset*> GetDefaultAssets() const { return DefaultAssets; }
	/**
	* 获取当前任务
	*/
	UFUNCTION(BlueprintPure)
	UTaskBase* GetCurrentTask() const { return CurrentTask; }
	/**
	* 设置当前任务
	*/
	UFUNCTION(BlueprintCallable)
	void SetCurrentTask(UTaskBase* InTask);
	/**
	* 通过GUID获取任务
	*/
	UFUNCTION(BlueprintPure)
	UTaskBase* GetTaskByGUID(const FString& InTaskGUID) const;
};
