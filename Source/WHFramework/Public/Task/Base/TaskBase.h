// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Task/TaskModuleTypes.h"
#include "Common/CommonModuleTypes.h"
#include "Common/Base/WHObject.h"
#include "SaveGame/Base/SaveDataAgentInterface.h"

#include "TaskBase.generated.h"

class UTaskAsset;

/**
 * 任务基类
 */
UCLASS(Blueprintable, hidecategories = (Default))
class WHFRAMEWORK_API UTaskBase : public UWHObject, public ISaveDataAgentInterface
{
	GENERATED_BODY()

public:
	UTaskBase();

	//////////////////////////////////////////////////////////////////////////
	/// Task
public:
#if WITH_EDITOR
	/**
	 * 任务构建
	 */
	virtual void OnGenerate();
	/**
	 * 任务取消构建
	 */
	virtual void OnUnGenerate();
#endif

public:
	/**
	* 任务状态改变
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnStateChanged")
	void K2_OnStateChanged(ETaskState InTaskState);
	UFUNCTION()
	virtual void OnStateChanged(ETaskState InTaskState);
	/**
	 * 任务初始化
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnInitialize")
	void K2_OnInitialize();
	UFUNCTION()
	virtual void OnInitialize();
	/**
	 * 任务还原
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRestore")
	void K2_OnRestore();
	UFUNCTION()
	virtual void OnRestore();
	/**
	 * 任务进入
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnEnter")
	void K2_OnEnter();
	UFUNCTION()
	virtual void OnEnter();
	/**
	 * 任务帧刷新
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRefresh")
	void K2_OnRefresh();
	UFUNCTION()
	virtual void OnRefresh();
	/**
	* 任务指引
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnGuide")
	void K2_OnGuide();
	UFUNCTION()
	virtual void OnGuide();
	/**
	 * 任务执行
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnExecute")
	void K2_OnExecute();
	UFUNCTION()
	virtual void OnExecute();
	/**
	 * 任务完成
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnComplete")
	void K2_OnComplete(ETaskExecuteResult InTaskExecuteResult);
	UFUNCTION()
	virtual void OnComplete(ETaskExecuteResult InTaskExecuteResult);
	/**
	 * 任务离开
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnLeave")
	void K2_OnLeave();
	UFUNCTION()
	virtual void OnLeave();

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnResume")
	void K2_OnResume();
	virtual void OnResume();
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnSuspend")
	void K2_OnSuspend();
	virtual void OnSuspend();
	void ClearTaskTimers();
	FTaskRuntimeSaveData CaptureRuntimeData();
	void ResumeRuntimeData(const FTaskRuntimeSaveData& Data);
	void SetTaskTimersPaused(bool bPaused);
	virtual void BeginDestroy() override;

public:
	/**
	* 还原任务
	*/
	UFUNCTION(BlueprintCallable)
	void Restore();
	/**
	* 进入任务
	*/
	UFUNCTION(BlueprintCallable)
	void Enter(bool bSetAsCurrent = false);
	/**
	* 刷新任务
	*/
	UFUNCTION(BlueprintCallable)
	void Refresh();
	/**
	* 刷新任务状态
	*/
	UFUNCTION(BlueprintCallable)
	void Restate();
	/**
	* 指引任务
	*/
	UFUNCTION(BlueprintCallable)
	void Guide();
	/**
	* 执行任务
	*/
	UFUNCTION(BlueprintCallable)
	void Execute();
	/**
	 * 完成任务
	 */
	UFUNCTION(BlueprintCallable)
	void Complete(ETaskExecuteResult InTaskExecuteResult = ETaskExecuteResult::Succeed);
	/**
	* 离开任务
	*/
	UFUNCTION(BlueprintCallable)
	void Leave();

protected:
	virtual void Serialize(FArchive& Ar) override;

	virtual void LoadData(const FParameter& InSaveData, EPhase InPhase) override;

	virtual FParameter ToData() override;

	virtual bool HasArchive() const override { return true; }

	//////////////////////////////////////////////////////////////////////////
	/// Name/Description
public:
	/// 任务GUID
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Name/Description")
	FString TaskGUID;
	/// 任务显示名称
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Name/Description")
	FText TaskDisplayName;
	/// 任务描述
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = "true"), Category = "Name/Description")
	FText TaskDescription;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag TaskCategory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FTaskReference> Prerequisites;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FTaskObjective> Objectives;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bSequentialSubTasks = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bRequireExplicitTurnIn = false;

	/** 运行时可解析的目标位置，角色与区域标识不受世界流送影响 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTaskTarget Target;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FTaskMarkerData Marker;

	UFUNCTION(BlueprintPure)
	ETaskStage GetTaskStage() const;

	UFUNCTION(BlueprintPure)
	int32 GetObjectiveProgress(FName ObjectiveID) const;

	UFUNCTION(BlueprintPure)
	bool AreObjectivesCompleted() const;

	UFUNCTION(BlueprintPure)
	bool ArePrerequisitesMet() const;

	void ApplyObjectiveEvent(FGameplayTag EventTag, FGameplayTag TargetTag, int32 Count, FPrimaryAssetId TargetAssetID, FName TargetName);

	UFUNCTION(BlueprintNativeEvent, BlueprintPure)
	bool CanTurnIn(AActor* InTarget, FText& OutReason) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	bool CommitTurnIn(AActor* InTarget);

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnReward")
	void K2_OnReward();
	virtual void OnReward();
	void GrantRewards();

protected:
	UPROPERTY(VisibleAnywhere)
	TMap<FName, int32> ObjectiveProgress;

	UPROPERTY(VisibleAnywhere)
	bool bRewardsGranted = false;

public:
	
	//////////////////////////////////////////////////////////////////////////
	/// Index/State
public:
	/// 任务索引
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Index/State")
	int32 TaskIndex;
	/// 任务层级
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Index/State")
	int32 TaskHierarchy;
	/// 任务状态
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Index/State")
	ETaskState TaskState;

	UPROPERTY(BlueprintAssignable)
	FOnTaskStateChanged OnTaskStateChanged;

public:
	/**
	* 获取任务状态
	*/
	UFUNCTION(BlueprintPure)
	ETaskState GetTaskState() const { return TaskState; }
	/**
	* 获取任务资产
	*/
	UFUNCTION(BlueprintPure)
	UTaskAsset* GetTaskAsset() const;
	/**
	* 是否是根任务
	*/
	UFUNCTION(BlueprintPure)
	bool IsRootTask() const { return TaskHierarchy == 0; }
	/**
	* 是否是当前任务
	*/
	UFUNCTION(BlueprintPure)
	bool IsCurrent() const;
	/**
	* 是否已进入
	*/
	UFUNCTION(BlueprintPure)
	bool IsEntered() const;
	/**
	* 是否正在执行
	*/
	UFUNCTION(BlueprintPure)
	bool IsExecuting() const;
	/**
	* 是否已完成
	*/
	UFUNCTION(BlueprintPure)
	bool IsCompleted(bool bCheckSubs = false) const;
	/**
	* 是否已成功
	*/
	UFUNCTION(BlueprintPure)
	bool IsSucceed(bool bCheckSubs = false) const;
	/**
	* 是否已离开
	*/
	UFUNCTION(BlueprintPure)
	bool IsLeaved(bool bCheckSubs = false) const;

	//////////////////////////////////////////////////////////////////////////
	/// Execute/Guide
public:
	/// 任务进入方式
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	ETaskEnterType TaskEnterType;
	/// 任务执行结果
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	ETaskExecuteResult TaskExecuteResult;
	/// 任务执行方式
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	ETaskExecuteType TaskExecuteType;
	/// 自动执行任务时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute/Guide")
	float AutoExecuteTaskTime;
	/// 任务完成方式
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	ETaskCompleteType TaskCompleteType;
	/// 自动完成任务时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute/Guide")
	float AutoCompleteTaskTime;
	/// 能否跳过任务
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute/Guide")
	bool bTaskSkipAble;
	/// 任务离开方式
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	ETaskLeaveType TaskLeaveType;
	/// 自动离开任务时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute/Guide")
	float AutoLeaveTaskTime;
	/// 任务指引类型 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	ETaskGuideType TaskGuideType;
	/// 任务指引间隔时间 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	float TaskGuideIntervalTime;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	bool bTaskTickEnabled = true;

protected:
	FTimerHandle StartGuideTimerHandle;
	
public:
	/**
	* 检测任务先决条件
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintPure)
	bool CheckTaskCondition(FString& OutInfo) const;
	/**
	* 检测任务能否跳过
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintPure)
	bool CheckTaskSkipAble(FString& OutInfo) const;
	/**
	* 检测任务完成进度
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintPure)
	float CheckTaskProgress(FString& OutInfo) const;

	//////////////////////////////////////////////////////////////////////////
	/// ParentTask
public:
	/// 根任务
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ParentTask")
	UTaskBase* RootTask;
	/// 父任务 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ParentTask")
	UTaskBase* ParentTask;

public:
	UFUNCTION(BlueprintPure)
	bool IsParentOf(UTaskBase* InTask) const;

	//////////////////////////////////////////////////////////////////////////
	/// SubTask
public:
	/// 是否合并子任务
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubTask")
	bool bMergeSubTask;
	/// 子任务
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "SubTask")
	TArray<UTaskBase*> SubTasks;
public:
	/**
	* 是否有子任务
	* @param bIgnoreMerge 是否忽略合并（ture => !bMergeSubTask）
	*/
	UFUNCTION(BlueprintPure)
	bool HasSubTask(bool bIgnoreMerge = true) const;
	/**
	 * 是否是子任务
	 */
	UFUNCTION(BlueprintPure)
	bool IsSubOf(UTaskBase* InTask) const;
	/**
	* 是否已进入所有子任务
	*/
	UFUNCTION(BlueprintPure)
	bool IsAllSubEntered() const;
	/**
	* 是否已完成所有子任务
	*/
	UFUNCTION(BlueprintPure)
	bool IsAllSubCompleted() const;
	virtual bool AreSubTasksReadyToComplete() const;
	/**
	* 是否已成功执行有子任务
	*/
	UFUNCTION(BlueprintPure)
	bool IsAllSubSucceed() const;
	/**
	* 是否已离开所有子任务
	*/
	UFUNCTION(BlueprintPure)
	bool IsAllSubLeaved() const;

protected:
	FTimerHandle AutoExecuteTimerHandle;
	FTimerHandle AutoLeaveTimerHandle;
	FTimerHandle AutoCompleteTimerHandle;

public:
	bool bRuntimeSelected;

	//////////////////////////////////////////////////////////////////////////
	/// Graph
public:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FVector2D GraphPosition = FVector2D::ZeroVector;
	UPROPERTY()
	bool bHasGraphPosition = false;
#endif
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
