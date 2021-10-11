// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Procedure/ProcedureModuleTypes.h"
#include "UObject/NoExportTypes.h"
#include "Debug/DebugModuleTypes.h"

#include "ProcedureBase.generated.h"

/**
 * 流程基类
 */
UCLASS(hidecategories = (Default, Tick, Replication, Rendering, Collision, Actor, Input, LOD, Cooking, Hidden, WorldPartition, Hlod, DataLayers))
class WHFRAMEWORK_API AProcedureBase : public AActor
{
	GENERATED_BODY()

public:
	AProcedureBase();

	//////////////////////////////////////////////////////////////////////////
	/// Procedure
public:
#if WITH_EDITOR
	/**
	 * 构建流程
	 */
	virtual void OnGenerate();
	/**
	 * 取消构建流程
	 */
	virtual void OnUnGenerate();
#endif
	/**
	* 构建流程列表项
	*/
	virtual void GenerateListItem(TSharedPtr<struct FProcedureListItem> OutProcedureListItem);
	/**
	* 更新流程列表项
	*/
	virtual void UpdateListItem(TSharedPtr<struct FProcedureListItem> OutProcedureListItem);

public:
	/**
	* 本地初始化
	*/
	virtual void NativeOnInitialize();
	/**
	 * 流程初始化
	 */
	UFUNCTION(BlueprintNativeEvent)
	void ServerOnInitialize();
	UFUNCTION(NetMulticast, Reliable)
	void MultiOnInitialize();
	UFUNCTION(BlueprintNativeEvent)
	void LocalOnInitialize();
	/**
	 * 流程准备
	 */
	UFUNCTION(BlueprintNativeEvent)
	void ServerOnPrepare();
	UFUNCTION(NetMulticast, Reliable)
	void MultiOnPrepare();
	UFUNCTION(BlueprintNativeEvent)
	void LocalOnPrepare();
	/**
	 * 进入流程
	 * @param InLastProcedure 上一个流程
	 */
	UFUNCTION(BlueprintNativeEvent)
	void ServerOnEnter(AProcedureBase* InLastProcedure);
	UFUNCTION(NetMulticast, Reliable)
	void MultiOnEnter(AProcedureBase* InLastProcedure);
	UFUNCTION(BlueprintNativeEvent)
	void LocalOnEnter(AProcedureBase* InLastProcedure);
	/**
	 * 流程帧刷新
	 */
	UFUNCTION(BlueprintNativeEvent)
	void ServerOnRefresh(float DeltaSeconds);
	UFUNCTION(NetMulticast, Reliable)
	void MultiOnRefresh(float DeltaSeconds);
	UFUNCTION(BlueprintNativeEvent)
	void LocalOnRefresh(float DeltaSeconds);
	/**
	* 流程指引
	*/
	UFUNCTION(BlueprintNativeEvent)
	void LocalOnGuide(const FName& InListenerTaskName);
	UFUNCTION(BlueprintNativeEvent)
	void LocalOnStopGuide(const FName& InListenerTaskName);
	/**
	 * 流程完成
	 */
	UFUNCTION(BlueprintNativeEvent)
	void ServerOnComplete(EProcedureExecuteResult InProcedureExecuteResult);
	UFUNCTION(NetMulticast, Reliable)
	void MultiOnComplete(EProcedureExecuteResult InProcedureExecuteResult);
	UFUNCTION(BlueprintNativeEvent)
	void LocalOnComplete(EProcedureExecuteResult InProcedureExecuteResult);
	/**
	 * 离开流程
	 * @param InNextProcedure 下一个流程
	 */
	UFUNCTION(BlueprintNativeEvent)
	void ServerOnLeave(AProcedureBase* InNextProcedure);
	UFUNCTION(NetMulticast, Reliable)
	void MultiOnLeave(AProcedureBase* InNextProcedure);
	UFUNCTION(BlueprintNativeEvent)
	void LocalOnLeave(AProcedureBase* InNextProcedure);
	/**
	 * 清理流程
	 */
	UFUNCTION(BlueprintNativeEvent)
	void ServerOnClear();
	UFUNCTION(NetMulticast, Reliable)
	void MultiOnClear();
	UFUNCTION(BlueprintNativeEvent)
	void LocalOnClear();
	/**
	 * 销毁流程
	 */
	UFUNCTION(BlueprintNativeEvent)
	void ServerOnDestroy();
	UFUNCTION(NetMulticast, Reliable)
	void MultiOnDestroy();
	UFUNCTION(BlueprintNativeEvent)
	void LocalOnDestroy();

public:
	/**
	* 准备流程
	*/
	UFUNCTION(BlueprintCallable)
	void ServerPrepare();
	UFUNCTION(NetMulticast, Reliable)
	void MultiPrepare();
	UFUNCTION(BlueprintCallable)
	void LocalPrepare();
	/**
	* 指引流程
	*/
	UFUNCTION(BlueprintCallable)
	void LocalGuide();
	UFUNCTION(BlueprintCallable)
	void LocalResetGuide();
	UFUNCTION(BlueprintCallable)
	void LocalResetGuideImpl();
	UFUNCTION(BlueprintCallable)
	void LocalStopGuide();
	/**
	 * 完成流程
	 */
	UFUNCTION(BlueprintCallable)
	void ServerComplete(EProcedureExecuteResult InProcedureExecuteResult = EProcedureExecuteResult::Succeed);
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable)
	void MultiComplete(EProcedureExecuteResult InProcedureExecuteResult = EProcedureExecuteResult::Succeed);
	UFUNCTION(BlueprintCallable)
	void LocalComplete(EProcedureExecuteResult InProcedureExecuteResult = EProcedureExecuteResult::Succeed);

	//////////////////////////////////////////////////////////////////////////
	/// Procedure Name/Description
public:
	/// 流程名称
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Replicated, Category = "Name/Description")
	FName ProcedureName;
	/// 流程显示名称
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Replicated, Category = "Name/Description")
	FText ProcedureDisplayName;
	/// 流程描述
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Replicated, Category = "Name/Description")
	FText ProcedureDescription;

	//////////////////////////////////////////////////////////////////////////
	/// Procedure Index/Type/State
public:
	/// 流程索引
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Replicated, Category = "Index/Type/State")
	int32 ProcedureIndex;
	/// 流程类型
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Replicated, Category = "Index/Type/State")
	EProcedureType ProcedureType;
	/// 流程状态
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, ReplicatedUsing = OnRep_ProcedureState, Category = "Index/Type/State")
	EProcedureState ProcedureState;
public:
	/**
	* 获取流程类型
	*/
	UFUNCTION(BlueprintPure)
	EProcedureType GetProcedureType() const { return ProcedureType; }
	/**
	* 获取流程状态
	*/
	UFUNCTION(BlueprintPure)
	EProcedureState GetProcedureState() const { return ProcedureState; }
	/**
	* 当流程状态改变
	*/
	UFUNCTION()
	void OnRep_ProcedureState();
	UFUNCTION(BlueprintNativeEvent)
	void OnChangeProcedureState(EProcedureState InProcedureState);
	
public:
	/// 本地流程状态
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Index/Type/State")
	EProcedureState LocalProcedureState;
public:
	/**
	* 获取本地流程状态
	*/
	UFUNCTION(BlueprintPure)
	EProcedureState GetLocalProcedureState() const { return LocalProcedureState; }
	/**
	* 当本地流程状态改变
	*/
	UFUNCTION(BlueprintNativeEvent)
	void OnChangeLocalProcedureState(EProcedureState InProcedureState);

	//////////////////////////////////////////////////////////////////////////
	/// Auto Enter/Leave
public:
	/// 自动进入流程
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Replicated, Category = "Auto Enter/Leave")
	bool bAutoEnterProcedure;
	/// 自动进入流程时间
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Replicated, meta = (EditCondition = "bAutoEnterProcedure"), Category = "Auto Enter/Leave")
	float AutoEnterProcedureTime;
	/// 自动离开流程
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Replicated, Category = "Auto Enter/Leave")
	bool bAutoLeaveProcedure;
	/// 自动离开流程时间
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Replicated, meta = (EditCondition = "bAutoLeaveProcedure"), Category = "Auto Enter/Leave")
	float AutoLeaveProcedureTime;

	//////////////////////////////////////////////////////////////////////////
	/// Execute/Guide
public:
	/// 流程执行类型
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Replicated, Category = "Execute/Guide")
	EProcedureExecuteType ProcedureExecuteType;
	/// 流程执行条件
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Replicated, Category = "Execute/Guide")
	EProcedureExecuteResult ProcedureExecuteCondition;
	/// 流程执行结果
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Replicated, Category = "Execute/Guide")
	EProcedureExecuteResult ProcedureExecuteResult;
	/// 本地流程执行结果
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Execute/Guide")
	EProcedureExecuteResult LocalProcedureExecuteResult;
	/// 流程指引类型 
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Replicated, Category = "Execute/Guide")
	EProcedureGuideType ProcedureGuideType;
	/// 流程指引间隔时间 
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Replicated, Category = "Execute/Guide")
	float ProcedureGuideIntervalTime;

protected:
	FTimerHandle StartGuideTimerHandle;
	FTimerHandle ResetGuideTimerHandle;
	
public:
	/**
	* 获取流程执行类型
	*/
	UFUNCTION(BlueprintPure)
	EProcedureExecuteType GetProcedureExecuteType() const;
	/**
	* 检测流程执行条件
	* 
	*/
	UFUNCTION(BlueprintPure)
	bool CheckProcedureCondition(AProcedureBase* InProcedure) const;
	/**
	* 检测本地流程执行条件
	* 
	*/
	UFUNCTION(BlueprintPure)
	bool CheckLocalProcedureCondition(AProcedureBase* InProcedure) const;

	//////////////////////////////////////////////////////////////////////////
	/// ParentProcedure
public:
	/// 父流程 
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Replicated, Category = "ParentProcedure")
	AProcedureBase* ParentProcedure;
	
	//////////////////////////////////////////////////////////////////////////
	/// SubProcedure
public:
	/// 是否合并子流程
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Replicated, Category = "SubProcedure")
	bool bMergeSubProcedure;
	/// 当前子流程索引
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Replicated, Category = "SubProcedure")
	int32 CurrentSubProcedureIndex;
	/// 当前本地子流程索引
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "SubProcedure")
	int32 CurrentLocalSubProcedureIndex;
	/// 子流程
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Replicated, Category = "SubProcedure")
	TArray<AProcedureBase*> SubProcedures;
public:
	/**
	* 是否有子流程
	* @param bIgnoreMerge 是否忽略合并（ture => !bMergeSubProcedure）
	*/
	UFUNCTION(BlueprintPure)
	bool HasSubProcedure(bool bIgnoreMerge = true) const;
	/**
	 * 获取当前子流程
	 */
	UFUNCTION(BlueprintPure)
	AProcedureBase* GetCurrentSubProcedure() const;
	/**
	* 获取当前本地子流程
	*/
	UFUNCTION(BlueprintPure)
	AProcedureBase* GetCurrentLocalSubProcedure() const;
	/**
	* 是否已完成所有子流程
	*/
	UFUNCTION(BlueprintPure)
	bool IsAllSubCompleted() const;
	/**
	* 是否已完成所有本地子流程
	*/
	UFUNCTION(BlueprintPure)
	bool IsAllLocalSubCompleted() const;
	/**
	* 是否已成功执行有子流程
	*/
	UFUNCTION(BlueprintPure)
	bool IsAllSubExecuteSucceed() const;
	/**
	* 是否已成功执行所有本地子流程
	*/
	UFUNCTION(BlueprintPure)
	bool IsAllLocalSubExecuteSucceed() const;

	//////////////////////////////////////////////////////////////////////////
	/// ProcedureTask
protected:
	/// 当前流程任务项索引
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "ProcedureTask")
	int32 CurrentProcedureTaskIndex;
	/// 流程任务项
	TArray<struct FProcedureTaskItem> ProcedureTaskItems;
	
	//////////////////////////////////////////////////////////////////////////
	/// ListenerTask
protected:
	/**
	* 获取子菜单名称
	*/
	UFUNCTION(BlueprintPure)
	FName GetListenerTaskName(const FString& InTaskName = TEXT("")) const;

public:
	/**
	* 是否有流程任务
	*/
	UFUNCTION(BlueprintPure)
	bool HasProcedureTask() const;
	/**
	* 是否已完成所有流程任务
	*/
	UFUNCTION(BlueprintPure)
	bool IsAllTaskCompleted() const;
	/**
	* 添加流程任务
	*/
	struct FProcedureTaskItem& AddProcedureTask(const FName InTaskName, float InDurationTime = 0.f, float InDelayTime = 1.f);

protected:
	FTimerHandle AutoEnterTimerHandle;
	FTimerHandle AutoLeaveTimerHandle;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
	
/**
* 流程任务项
*/
struct FProcedureTaskItem
{
public:
	FProcedureTaskItem()
	{
		TaskState = EProcedureTaskState::None;
		TaskName = NAME_None;
		DurationTime = 0.f;
		DelayTime = 1.f;
		RemainTime = 0.f;
	}

	FProcedureTaskItem(const FName InTaskName, float InDurationTime, float InDelayTime)
	{
		TaskState = EProcedureTaskState::None;
		TaskName = InTaskName;
		DurationTime = InDurationTime;
		DelayTime = InDelayTime;
		RemainTime = 0.f;
	}

public:
	/// 任务状态
	EProcedureTaskState TaskState;
	/// 任务名称
	FName TaskName;
	
	/// 持续时间
	float DurationTime;
	/// 延迟时间
	float DelayTime;
	/// 剩余时间
	float RemainTime;

	DECLARE_DELEGATE_RetVal(bool, FOnCheckProcedureTaskDelegate);
	/// 任务检测代理
	FOnCheckProcedureTaskDelegate OnCheckDelegate;

	DECLARE_MULTICAST_DELEGATE(FOnExecuteProcedureTaskDelegate);
	/// 任务执行代理
	FOnExecuteProcedureTaskDelegate OnExecuteDelegate;

public:
	template <typename... VarTypes>
	FProcedureTaskItem& AddOnExecuteTaskFunc(class AProcedureBase* InProcedure, const FName InFuncName, VarTypes... Vars)
	{
		if (InProcedure)
		{
			UFunction* Func = InProcedure->FindFunction(InFuncName);
			if (ensureEditor(Func))
			{
				OnExecuteDelegate.Add(FOnExecuteProcedureTaskDelegate::FDelegate::CreateUFunction(InProcedure, InFuncName, Vars...));
			}
		}
		return *this;
	}

	bool IsValid() const
	{
		return !TaskName.IsNone();
	}

	void Prepare()
	{
		TaskState = EProcedureTaskState::Preparing;
		RemainTime = DelayTime;
	}

	void TryExecute(float DeltaSeconds)
	{
		if(!Refresh(DeltaSeconds)) return;

		TaskState = EProcedureTaskState::Executing;
		RemainTime = DurationTime;

		OnExecuteDelegate.Broadcast();

		WH_LOG(WHProcedure, Log, TEXT("ExecuteProcedureTask : TaskName = %s"), *TaskName.ToString());
	}

	void TryComplete(float DeltaSeconds)
	{
		if(!OnCheckDelegate.IsBound())
		{
			if(Refresh(DeltaSeconds))
			{
				TaskState = EProcedureTaskState::Completed;
			}
		}
		else if(OnCheckDelegate.Execute())
		{
			TaskState = EProcedureTaskState::Completed;
		}
	}

private:
	bool Refresh(float DeltaSeconds)
	{
		RemainTime -= DeltaSeconds;
		return RemainTime <= 0;
	}
};

/**
 * 流程列表项
 */ 
struct FProcedureListItem : public TSharedFromThis<FProcedureListItem>
{
public:
	FProcedureListItem()
	{
		Procedure = nullptr;
		ParentListItem = nullptr; 
		SubListItems = TArray<TSharedPtr<FProcedureListItem>>();
	}

	AProcedureBase* Procedure;

	TSharedPtr<FProcedureListItem> ParentListItem;

	TArray<TSharedPtr<FProcedureListItem>> SubListItems;

public:
	int32& GetProcedureIndex() const
	{
		return Procedure->ProcedureIndex;
	}

	AProcedureBase* GetParentProcedure() const
	{
		return Procedure->ParentProcedure;
	}
	
	TArray<AProcedureBase*>& GetSubProcedures()const
	{
		return Procedure->SubProcedures;
	}
	
	void GetSubProcedureNum(int32& OutNum) const
	{
		OutNum += SubListItems.Num();
		for(auto Iter : SubListItems)
		{
			Iter->GetSubProcedureNum(OutNum);
		}
	}

	TArray<AProcedureBase*>& GetParentSubProcedures() const
	{
		return ParentListItem->GetSubProcedures();
	}
	
	TArray<TSharedPtr<FProcedureListItem>>& GetParentSubListItems() const
	{
		return ParentListItem->SubListItems;
	}
};
