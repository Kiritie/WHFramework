// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Task/TaskModuleTypes.h"
#include "Common/CommonTypes.h"
#include "Common/Base/WHObject.h"
#include "SaveGame/Base/SaveDataAgentInterface.h"

#include "TaskBase.generated.h"

class UTaskAsset;
class ACameraActorBase;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FTaskListItemStates
{
	GENERATED_BODY()

public:
	UPROPERTY()
	bool bExpanded;

	UPROPERTY()
	bool bSelected;

	FORCEINLINE FTaskListItemStates()
	{
		bExpanded = true;
		bSelected = false;
	}
};

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
	void RefreshState();
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

	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;

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
	* 是否已离开
	*/
	UFUNCTION(BlueprintPure)
	bool IsLeaved() const;

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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SubTask")
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
	* 是否已完成所有子任务
	*/
	UFUNCTION(BlueprintPure)
	bool IsAllSubCompleted() const;
	/**
	* 是否已成功执行有子任务
	*/
	UFUNCTION(BlueprintPure)
	bool IsAllSubExecuteSucceed() const;

protected:
	FTimerHandle AutoExecuteTimerHandle;
	FTimerHandle AutoLeaveTimerHandle;
	FTimerHandle AutoCompleteTimerHandle;

	//////////////////////////////////////////////////////////////////////////
	/// TaskListItem
public:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FTaskListItemStates TaskListItemStates;
#endif
#if WITH_EDITOR
	/**
	* 构建任务列表项
	*/
	virtual bool GenerateListItem(TSharedPtr<struct FTaskListItem> OutTaskListItem, const FString& InFilterText = TEXT(""));
	/**
	* 更新任务列表项
	*/
	virtual void UpdateListItem(TSharedPtr<struct FTaskListItem> OutTaskListItem);

	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

/**
 * 任务列表项
 */ 
#if WITH_EDITOR
struct FTaskListItem : public TSharedFromThis<FTaskListItem>
{
public:
	FTaskListItem()
	{
		Task = nullptr;
		ParentListItem = nullptr; 
		SubListItems = TArray<TSharedPtr<FTaskListItem>>();
	}

	UTaskBase* Task;

	TSharedPtr<FTaskListItem> ParentListItem;

	TArray<TSharedPtr<FTaskListItem>> SubListItems;

public:
	FTaskListItemStates& GetStates() const
	{
		return Task->TaskListItemStates;
	}

	int32& GetTaskIndex() const
	{
		return Task->TaskIndex;
	}

	UTaskBase* GetParentTask() const
	{
		return Task->ParentTask;
	}
	
	TArray<UTaskBase*>& GetSubTasks()const
	{
		return Task->SubTasks;
	}
	
	void GetSubTaskNum(int32& OutNum) const
	{
		OutNum += SubListItems.Num();
		for(auto Iter : SubListItems)
		{
			Iter->GetSubTaskNum(OutNum);
		}
	}

	TArray<UTaskBase*>& GetParentSubTasks() const
	{
		return ParentListItem->GetSubTasks();
	}
	
	TArray<TSharedPtr<FTaskListItem>>& GetParentSubListItems() const
	{
		return ParentListItem->SubListItems;
	}
};
#endif
