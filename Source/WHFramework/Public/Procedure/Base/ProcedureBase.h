// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Procedure/ProcedureModuleTypes.h"
#include "Debug/DebugModuleTypes.h"

#include "ProcedureBase.generated.h"

/**
 * 流程基类
 */
UCLASS(Blueprintable, meta = (ShowWorldContextPin), hidecategories = (Default, Tick, Replication, Rendering, Collision, Actor, Input, LOD, Cooking, Hidden, WorldPartition, Hlod, DataLayers))
class WHFRAMEWORK_API UProcedureBase : public UObject
{
	GENERATED_BODY()

public:
	UProcedureBase();

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

public:
	/**
	 * 流程初始化
	 */
	UFUNCTION(BlueprintNativeEvent)
	void OnInitialize();
	/**
	 * 流程还原
	 */
	UFUNCTION(BlueprintNativeEvent)
	void OnRestore();
	/**
	 * 流程进入
	 * @param InLastProcedure 上一个流程
	 */
	UFUNCTION(BlueprintNativeEvent)
	void OnEnter(UProcedureBase* InLastProcedure);
	/**
	 * 流程帧刷新
	 */
	UFUNCTION(BlueprintNativeEvent)
	void OnRefresh(float DeltaSeconds);
	/**
	* 流程指引
	*/
	UFUNCTION(BlueprintNativeEvent)
	void OnGuide();
	/**
	 * 流程执行
	 */
	UFUNCTION(BlueprintNativeEvent)
	void OnExecute();
	/**
	 * 流程完成
	 */
	UFUNCTION(BlueprintNativeEvent)
	void OnComplete(EProcedureExecuteResult InProcedureExecuteResult);
	/**
	 * 流程离开
	 */
	UFUNCTION(BlueprintNativeEvent)
	void OnLeave();

public:
	/**
	* 还原流程
	*/
	UFUNCTION(BlueprintCallable)
	void Restore();
	/**
	* 进入流程
	*/
	UFUNCTION(BlueprintCallable)
	void Enter();
	/**
	* 指引流程
	*/
	UFUNCTION(BlueprintCallable)
	void Guide();
	/**
	* 执行流程
	*/
	UFUNCTION(BlueprintCallable)
	void Execute();
	/**
	 * 完成流程
	 */
	UFUNCTION(BlueprintCallable)
	void Complete(EProcedureExecuteResult InProcedureExecuteResult = EProcedureExecuteResult::Succeed);
	/**
	* 离开流程
	*/
	UFUNCTION(BlueprintCallable)
	void Leave();

	//////////////////////////////////////////////////////////////////////////
	/// Operation Target
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target")
	AActor* OperationTarget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target|Camera View")
	FVector CameraViewOffset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target|Camera View")
	float CameraViewYaw;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target|Camera View")
	float CameraViewPitch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target|Camera View")
	float CameraViewDistance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target|Camera View")
	bool bCameraViewInstant;
	
protected:
#if WITH_EDITOR
	/**
	* 获取摄像机视角
	*/
	UFUNCTION(CallInEditor, Category = "Operation Target")
	void GetCameraView();
#endif
	/**
	* 还原摄像机视角
	*/
	UFUNCTION(BlueprintCallable)
	void ResetCameraView();

	//////////////////////////////////////////////////////////////////////////
	/// Name/Description
public:
	/// 流程名称
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Name/Description")
	FName ProcedureName;
	/// 流程显示名称
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Name/Description")
	FText ProcedureDisplayName;
	/// 流程描述
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Name/Description")
	FText ProcedureDescription;

	//////////////////////////////////////////////////////////////////////////
	/// Index/Type/State
public:
	/// 流程索引
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Index/Type/State")
	int32 ProcedureIndex;
	/// 流程类型
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Index/Type/State")
	EProcedureType ProcedureType;
	/// 流程状态
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Index/Type/State")
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
	void ChangeProcedureState(EProcedureState InProcedureState);
	UFUNCTION(BlueprintNativeEvent)
	void OnChangeProcedureState(EProcedureState InProcedureState);

	//////////////////////////////////////////////////////////////////////////
	/// Execute/Guide
public:
	/// 流程执行条件
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	EProcedureExecuteResult ProcedureExecuteCondition;
	/// 流程执行结果
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	EProcedureExecuteResult ProcedureExecuteResult;
	/// 流程执行方式
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	EProcedureExecuteType ProcedureExecuteType;
	/// 自动执行流程时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute/Guide")
	float AutoExecuteProcedureTime;
	/// 流程完成方式
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	EProcedureCompleteType ProcedureCompleteType;
	/// 自动完成流程时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute/Guide")
	float AutoCompleteProcedureTime;
	/// 流程离开方式
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	EProcedureLeaveType ProcedureLeaveType;
	/// 自动离开流程时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute/Guide")
	float AutoLeaveProcedureTime;
	/// 流程指引类型 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	EProcedureGuideType ProcedureGuideType;
	/// 流程指引间隔时间 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	float ProcedureGuideIntervalTime;

protected:
	FTimerHandle StartGuideTimerHandle;
	
public:
	/**
	* 检测流程执行条件
	*/
	UFUNCTION(BlueprintPure)
	bool CheckProcedureCondition(UProcedureBase* InProcedure) const;
	
	UFUNCTION(BlueprintPure)
	EProcedureExecuteType GetProcedureExecuteType() const;

	UFUNCTION(BlueprintPure)
	EProcedureLeaveType GetProcedureLeaveType() const;

	UFUNCTION(BlueprintPure)
	EProcedureCompleteType GetProcedureCompleteType() const;

	//////////////////////////////////////////////////////////////////////////
	/// ParentProcedure
public:
	/// 父流程 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ParentProcedure")
	UProcedureBase* ParentProcedure;

public:
	UFUNCTION(BlueprintPure)
	bool IsParentOf(UProcedureBase* InProcedure) const;

	//////////////////////////////////////////////////////////////////////////
	/// SubProcedure
public:
	/// 是否合并子流程
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubProcedure")
	bool bMergeSubProcedure;
	/// 当前子流程索引
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SubProcedure")
	int32 CurrentSubProcedureIndex;
	/// 子流程
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SubProcedure")
	TArray<UProcedureBase*> SubProcedures;
public:
	UFUNCTION(BlueprintPure)
	bool IsSubOf(UProcedureBase* InProcedure) const;
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
	UProcedureBase* GetCurrentSubProcedure() const;
	/**
	* 是否已完成所有子流程
	*/
	UFUNCTION(BlueprintPure)
	bool IsAllSubCompleted() const;
	/**
	* 是否已成功执行有子流程
	*/
	UFUNCTION(BlueprintPure)
	bool IsAllSubExecuteSucceed() const;

	//////////////////////////////////////////////////////////////////////////
	/// ProcedureTask
protected:
	/// 当前流程任务项索引
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProcedureTask")
	int32 CurrentProcedureTaskIndex;
	/// 流程任务项
	TArray<struct FProcedureTaskItem> ProcedureTaskItems;

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
	FTimerHandle AutoExecuteTimerHandle;
	FTimerHandle AutoLeaveTimerHandle;
	FTimerHandle AutoCompleteTimerHandle;

	//////////////////////////////////////////////////////////////////////////
	/// ProcedureListItem
public:
	UPROPERTY()
	FProcedureListItemStates ProcedureListItemStates;
public:
	/**
	* 构建流程列表项
	*/
	virtual void GenerateListItem(TSharedPtr<struct FProcedureListItem> OutProcedureListItem);
	/**
	* 更新流程列表项
	*/
	virtual void UpdateListItem(TSharedPtr<struct FProcedureListItem> OutProcedureListItem);

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
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
	FProcedureTaskItem& AddOnExecuteTaskFunc(class UProcedureBase* InProcedure, const FName InFuncName, VarTypes... Vars)
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

	UProcedureBase* Procedure;

	TSharedPtr<FProcedureListItem> ParentListItem;

	TArray<TSharedPtr<FProcedureListItem>> SubListItems;

public:
	FProcedureListItemStates& GetStates() const
	{
		return Procedure->ProcedureListItemStates;
	}

	int32& GetProcedureIndex() const
	{
		return Procedure->ProcedureIndex;
	}

	UProcedureBase* GetParentProcedure() const
	{
		return Procedure->ParentProcedure;
	}
	
	TArray<UProcedureBase*>& GetSubProcedures()const
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

	TArray<UProcedureBase*>& GetParentSubProcedures() const
	{
		return ParentListItem->GetSubProcedures();
	}
	
	TArray<TSharedPtr<FProcedureListItem>>& GetParentSubListItems() const
	{
		return ParentListItem->SubListItems;
	}
};
