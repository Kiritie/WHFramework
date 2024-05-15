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
	virtual void Serialize(FArchive& Ar) override;

	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual void UnloadData(EPhase InPhase) override;

	virtual FSaveData* ToData() override;

	virtual bool HasArchive() const override { return true; }

public:
	virtual FString GetModuleDebugMessage() override;

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

public:
	UFUNCTION(BlueprintPure)
	bool IsAllTaskCompleted();

	//////////////////////////////////////////////////////////////////////////
	/// Task Stats
protected:
	/// 默认流程资产 
	UPROPERTY(EditAnywhere, Category = "TaskModule|Task Stats")
	UTaskAsset* DefaultAsset;
	/// 当前流程资产 
	UPROPERTY(VisibleAnywhere, Category = "TaskModule|Task Stats")
	UTaskAsset* CurrentAsset;
	/// 当前任务 
	UPROPERTY(VisibleAnywhere, Category = "TaskModule|Task Stats")
	UTaskBase* CurrentTask;

public:
	/**
	* 获取默认资产
	*/
	UFUNCTION(BlueprintPure)
	UTaskAsset* GetDefaultAsset() const { return DefaultAsset; }
	/**
	* 获取当前资产
	*/
	UFUNCTION(BlueprintPure)
	UTaskAsset* GetCurrentAsset() const { return CurrentAsset; }
	/**
	* 设置当前资产
	*/
	UFUNCTION(BlueprintCallable)
	void SetCurrentAsset(UTaskAsset* InTaskAsset, bool bInAutoEnterFirst = false);
	/**
	* 获取初始任务
	*/
	UFUNCTION(BlueprintPure)
	UTaskBase* GetFirstTask() const { return CurrentAsset ? CurrentAsset->FirstTask : nullptr; }
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
	TArray<UTaskBase*> GetRootTasks() const { return CurrentAsset ? CurrentAsset->RootTasks : TArray<UTaskBase*>(); }
	/**
	* 获取任务Map
	*/
	UFUNCTION(BlueprintPure)
	TMap<FString, UTaskBase*> GetTaskMap() const { return CurrentAsset ? CurrentAsset->TaskMap : TMap<FString, UTaskBase*>(); }
	/**
	* 通过GUID获取任务
	*/
	UFUNCTION(BlueprintPure)
	UTaskBase* GetTaskByGUID(const FString& InTaskGUID) const { return *GetTaskMap().Find(InTaskGUID); }
};
