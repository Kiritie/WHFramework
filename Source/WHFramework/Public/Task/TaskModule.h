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

	UPROPERTY(BlueprintAssignable)
	FOnTaskAssetsChanged OnTaskAssetsChanged;

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
	virtual void LoadData(const FParameter& InSaveData, EPhase InPhase) override;

	virtual void UnloadData(EPhase InPhase) override;

	virtual FParameter ToData() override;

public:
	virtual FString GetModuleDebugMessage() override;

	//////////////////////////////////////////////////////////////////////////
	/// TaskModule
public:
	UFUNCTION(BlueprintPure)
	UTaskAsset* GetAsset(UTaskAsset* InAsset) const;

	UFUNCTION(BlueprintPure)
	UTaskAsset* GetAssetByInstance(UTaskAsset* InAsset, FGuid InInstanceID) const;

	UFUNCTION(BlueprintPure)
	TArray<UTaskAsset*> GetAssetsByAgent(FGuid InAgentID) const;
	
	UFUNCTION(BlueprintCallable)
	UTaskAsset* AddAsset(UTaskAsset* InAsset);

	/** 根据任务资产模板创建独立运行时副本 */
	UFUNCTION(BlueprintCallable)
	UTaskAsset* CreateAsset(UTaskAsset* InAsset, FGuid InAgentID);
	
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

	UFUNCTION(BlueprintPure)
	UTaskBase* ResolveTask(const FTaskReference& Reference, UTaskAsset* InContext = nullptr) const;

	UFUNCTION(BlueprintCallable)
	UTaskBase* EnsureTask(const FTaskReference& Reference);

	UFUNCTION(BlueprintCallable)
	bool TurnInTask(UTaskBase* InTask, AActor* InTarget);

	UFUNCTION(BlueprintCallable)
	void ReportTaskEvent(FGameplayTag InEventTag, FGameplayTag InTargetTag, int32 InCount = 1, FPrimaryAssetId InTargetAssetID = FPrimaryAssetId(), FName InTargetName = NAME_None);

	UFUNCTION(BlueprintCallable)
	void RefreshMarkers() { RequestTaskMarkersRefresh(); }

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

	UPROPERTY(Transient)
	TMap<UTaskBase*, FTaskRuntimeSaveData> PendingResume;
	bool bLoadingTasks = false;
	UTaskBase* TurningInTask = nullptr;
	TSet<FGuid> TaskMarkerIDs;
	int32 TaskMutationDepth = 0;
	bool bTaskMarkersDirty = false;
	bool bAutomaticTaskEntryDirty = false;
	bool bResolvingAutomaticTasks = false;
	void ClearRuntimeAssets();
	UTaskBase* ResolveRuntimeTask(UTaskBase* Task) const;
	UTaskAsset* AddAssetInternal(UTaskAsset* InAsset, FGuid InInstanceID, FGuid InAgentID);
	void BeginTaskMutation();
	void EndTaskMutation();
	void RequestTaskMarkersRefresh();
	void EnterEligibleAutomaticTasks();
	void EnterEligibleAutomaticTasks(UTaskBase* InTask);
	void RefreshTaskMarkers();
	void RemoveTaskMarkers(const UTaskAsset* InAsset = nullptr);
	static FGuid MakeTaskMarkerID(const UTaskAsset* Asset, const FString& GUID);
	static FString GetSaveKey(const UTaskAsset* Asset, const FString& GUID);

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

	UFUNCTION(BlueprintPure)
	UTaskBase* GetGuidanceTask() const;
	/**
	* 设置当前任务
	*/
	UFUNCTION(BlueprintCallable)
	void SetCurrentTask(UTaskBase* InTask);
	/**
	* 是否有指定GUID的任务
	*/
	UFUNCTION(BlueprintPure)
	bool IsExistTaskByGUID(const FString& InTaskGUID) const;
	/**
	* 通过GUID获取任务
	*/
	UFUNCTION(BlueprintPure)
	UTaskBase* GetTaskByGUID(const FString& InTaskGUID) const;
};
