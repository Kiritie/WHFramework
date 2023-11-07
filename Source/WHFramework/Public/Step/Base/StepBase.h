// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Step/StepModuleTypes.h"
#include "Debug/DebugModuleTypes.h"
#include "Common/Base/WHObject.h"
#include "Math/MathTypes.h"
#include "Camera/CameraModuleTypes.h"

#include "StepBase.generated.h"

class ACameraActorBase;
/**
 * 步骤基类
 */
UCLASS(Blueprintable, hidecategories = (Default))
class WHFRAMEWORK_API UStepBase : public UWHObject
{
	GENERATED_BODY()

public:
	UStepBase();

	//////////////////////////////////////////////////////////////////////////
	/// Step
public:
#if WITH_EDITOR
	/**
	 * 步骤构建
	 */
	virtual void OnGenerate();
	/**
	 * 步骤取消构建
	 */
	virtual void OnUnGenerate();
#endif

public:
	/**
	* 步骤状态改变
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnStateChanged")
	void K2_OnStateChanged(EStepState InStepState);
	UFUNCTION()
	virtual void OnStateChanged(EStepState InStepState);
	/**
	 * 步骤初始化
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnInitialize")
	void K2_OnInitialize();
	UFUNCTION()
	virtual void OnInitialize();
	/**
	 * 步骤还原
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRestore")
	void K2_OnRestore();
	UFUNCTION()
	virtual void OnRestore();
	/**
	 * 步骤进入
	 * @param InLastStep 上一个步骤
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnEnter")
	void K2_OnEnter(UStepBase* InLastStep);
	UFUNCTION()
	virtual void OnEnter(UStepBase* InLastStep);
	/**
	 * 步骤帧刷新
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRefresh")
	void K2_OnRefresh();
	UFUNCTION()
	virtual void OnRefresh();
	/**
	* 步骤指引
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnGuide")
	void K2_OnGuide();
	UFUNCTION()
	virtual void OnGuide();
	/**
	 * 步骤执行
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnExecute")
	void K2_OnExecute();
	UFUNCTION()
	virtual void OnExecute();
	/**
	 * 步骤完成
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnComplete")
	void K2_OnComplete(EStepExecuteResult InStepExecuteResult);
	UFUNCTION()
	virtual void OnComplete(EStepExecuteResult InStepExecuteResult);
	/**
	 * 步骤离开
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnLeave")
	void K2_OnLeave();
	UFUNCTION()
	virtual void OnLeave();

public:
	/**
	* 还原步骤
	*/
	UFUNCTION(BlueprintCallable)
	void Restore();
	/**
	* 进入步骤
	*/
	UFUNCTION(BlueprintCallable)
	void Enter();
	/**
	* 进入步骤
	*/
	UFUNCTION(BlueprintCallable)
	void Refresh();
	/**
	* 指引步骤
	*/
	UFUNCTION(BlueprintCallable)
	void Guide();
	/**
	* 执行步骤
	*/
	UFUNCTION(BlueprintCallable)
	void Execute();
	/**
	 * 完成步骤
	 */
	UFUNCTION(BlueprintCallable)
	void Complete(EStepExecuteResult InStepExecuteResult = EStepExecuteResult::Succeed);
	/**
	* 离开步骤
	*/
	UFUNCTION(BlueprintCallable)
	void Leave();

	//////////////////////////////////////////////////////////////////////////
	/// Name/Description
public:
	/// 步骤名称
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Name/Description")
	FString StepGUID;
	/// 步骤显示名称
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Name/Description")
	FText StepDisplayName;
	/// 步骤描述
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = "true"), Category = "Name/Description")
	FText StepDescription;

	//////////////////////////////////////////////////////////////////////////
	/// Index/State
public:
	/// 是否为开始步骤
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Index/State")
	bool bFirstStep;
	/// 步骤索引
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Index/State")
	int32 StepIndex;
	/// 步骤层级
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Index/State")
	int32 StepHierarchy;
	/// 步骤状态
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Index/State")
	EStepState StepState;

	UPROPERTY(BlueprintAssignable)
	FOnStepStateChanged OnStepStateChanged;

public:
	/**
	* 获取步骤状态
	*/
	UFUNCTION(BlueprintPure)
	EStepState GetStepState() const { return StepState; }
	/**
	* 是否是根步骤
	*/
	UFUNCTION(BlueprintPure)
	bool IsRootStep() const { return StepHierarchy == 0; }
	/**
	* 是否已进入
	*/
	UFUNCTION(BlueprintPure)
	bool IsEntered() const;
	/**
	* 是否已完成
	*/
	UFUNCTION(BlueprintPure)
	bool IsCompleted(bool bCheckSubs = false) const;
	/**
	* 能否跳过
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintPure)
	bool IsSkipAble() const;

	//////////////////////////////////////////////////////////////////////////
	/// Operation Target
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation Target")
	AActor* OperationTarget;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditConditionHides, EditCondition = "OperationTarget != nullptr"), Category = "Operation Target")
	bool bTrackTarget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target|Camera View")
	ACameraActorBase* CameraViewPawn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target|Camera View")
	EStepCameraViewMode CameraViewMode;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target|Camera View")
	EStepCameraViewSpace CameraViewSpace;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditConditionHides, EditCondition = "CameraViewMode == EStepCameraViewMode::Duration"), Category = "Operation Target|Camera View")
	EEaseType CameraViewEaseType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditConditionHides, EditCondition = "CameraViewMode == EStepCameraViewMode::Duration"), Category = "Operation Target|Camera View")
	float CameraViewDuration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target|Camera View")
	FVector CameraViewOffset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target|Camera View")
	float CameraViewYaw;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target|Camera View")
	float CameraViewPitch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target|Camera View")
	float CameraViewDistance;

public:
#if WITH_EDITOR
	/**
	* 获取摄像机视角
	*/
	void GetCameraView();
	/**
	* 设置摄像机视角
	*/
	void SetCameraView(FCameraParams InCameraParams);
#endif
	/**
	* 还原摄像机视角
	*/
	UFUNCTION(BlueprintCallable)
	void ResetCameraView();

	//////////////////////////////////////////////////////////////////////////
	/// Execute/Guide
public:
	/// 步骤执行条件
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	EStepExecuteResult StepExecuteCondition;
	/// 步骤执行结果
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	EStepExecuteResult StepExecuteResult;
	/// 步骤执行方式
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	EStepExecuteType StepExecuteType;
	/// 自动执行步骤时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute/Guide")
	float AutoExecuteStepTime;
	/// 步骤完成方式
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	EStepCompleteType StepCompleteType;
	/// 自动完成步骤时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute/Guide")
	float AutoCompleteStepTime;
	/// 步骤离开方式
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	EStepLeaveType StepLeaveType;
	/// 自动离开步骤时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute/Guide")
	float AutoLeaveStepTime;
	/// 步骤指引类型 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	EStepGuideType StepGuideType;
	/// 步骤指引间隔时间 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	float StepGuideIntervalTime;

protected:
	FTimerHandle StartGuideTimerHandle;
	
public:
	/**
	* 检测步骤执行条件
	*/
	UFUNCTION(BlueprintPure)
	bool CheckStepCondition(UStepBase* InStep) const;
	
	UFUNCTION(BlueprintPure)
	EStepExecuteType GetStepExecuteType() const;

	UFUNCTION(BlueprintPure)
	EStepLeaveType GetStepLeaveType() const;

	UFUNCTION(BlueprintPure)
	EStepCompleteType GetStepCompleteType() const;

	//////////////////////////////////////////////////////////////////////////
	/// ParentStep
public:
	/// 根步骤
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ParentStep")
	UStepBase* RootStep;
	/// 父步骤 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ParentStep")
	UStepBase* ParentStep;

public:
	UFUNCTION(BlueprintPure)
	bool IsParentOf(UStepBase* InStep) const;

	//////////////////////////////////////////////////////////////////////////
	/// SubStep
public:
	/// 是否合并子步骤
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubStep")
	bool bMergeSubStep;
	/// 当前子步骤索引
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SubStep")
	int32 CurrentSubStepIndex;
	/// 子步骤
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SubStep")
	TArray<UStepBase*> SubSteps;
public:
	/**
	* 是否有子步骤
	* @param bIgnoreMerge 是否忽略合并（ture => !bMergeSubStep）
	*/
	UFUNCTION(BlueprintPure)
	bool HasSubStep(bool bIgnoreMerge = true) const;
	/**
	 * 获取当前子步骤
	 */
	UFUNCTION(BlueprintPure)
	UStepBase* GetCurrentSubStep() const;
	/**
	 * 是否是子步骤
	 */
	UFUNCTION(BlueprintPure)
	bool IsSubOf(UStepBase* InStep) const;
	/**
	* 是否已完成所有子步骤
	*/
	UFUNCTION(BlueprintPure)
	bool IsAllSubCompleted() const;
	/**
	* 是否已成功执行有子步骤
	*/
	UFUNCTION(BlueprintPure)
	bool IsAllSubExecuteSucceed() const;

	//////////////////////////////////////////////////////////////////////////
	/// StepTask
protected:
	/// 当前步骤任务项索引
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StepTask")
	int32 CurrentStepTaskIndex;
	/// 步骤任务项
	TArray<struct FStepTaskItem> StepTaskItems;

public:
	/**
	* 是否有步骤任务
	*/
	UFUNCTION(BlueprintPure)
	bool HasStepTask() const;
	/**
	* 是否已完成所有步骤任务
	*/
	UFUNCTION(BlueprintPure)
	bool IsAllTaskCompleted() const;
	/**
	* 添加步骤任务
	*/
	struct FStepTaskItem& AddStepTask(const FName InTaskName, float InDurationTime = 0.f, float InDelayTime = 1.f);

protected:
	FTimerHandle AutoExecuteTimerHandle;
	FTimerHandle AutoLeaveTimerHandle;
	FTimerHandle AutoCompleteTimerHandle;

	//////////////////////////////////////////////////////////////////////////
	/// StepListItem
public:
	UPROPERTY()
	FStepListItemStates StepListItemStates;
public:
	/**
	* 构建步骤列表项
	*/
	virtual void GenerateListItem(TSharedPtr<struct FStepListItem> OutStepListItem);
	/**
	* 更新步骤列表项
	*/
	virtual void UpdateListItem(TSharedPtr<struct FStepListItem> OutStepListItem);

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
	
/**
* 步骤任务项
*/
struct FStepTaskItem
{
public:
	FStepTaskItem()
	{
		TaskState = EStepTaskState::None;
		TaskName = NAME_None;
		DurationTime = 0.f;
		DelayTime = 1.f;
		RemainTime = 0.f;
	}

	FStepTaskItem(const FName InTaskName, float InDurationTime, float InDelayTime)
	{
		TaskState = EStepTaskState::None;
		TaskName = InTaskName;
		DurationTime = InDurationTime;
		DelayTime = InDelayTime;
		RemainTime = 0.f;
	}

public:
	/// 任务状态
	EStepTaskState TaskState;
	/// 任务名称
	FName TaskName;
	
	/// 持续时间
	float DurationTime;
	/// 延迟时间
	float DelayTime;
	/// 剩余时间
	float RemainTime;

	DECLARE_DELEGATE_RetVal(bool, FOnCheckStepTaskDelegate);
	/// 任务检测代理
	FOnCheckStepTaskDelegate OnCheckDelegate;

	DECLARE_MULTICAST_DELEGATE(FOnExecuteStepTaskDelegate);
	/// 任务执行代理
	FOnExecuteStepTaskDelegate OnExecuteDelegate;

public:
	template <typename... VarTypes>
	FStepTaskItem& AddOnExecuteTaskFunc(class UStepBase* InStep, const FName InFuncName, VarTypes... Vars)
	{
		if (InStep)
		{
			UFunction* Func = InStep->FindFunction(InFuncName);
			if (WHEnsureEditor(Func))
			{
				OnExecuteDelegate.Add(FOnExecuteStepTaskDelegate::FDelegate::CreateUFunction(InStep, InFuncName, Vars...));
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
		TaskState = EStepTaskState::Preparing;
		RemainTime = DelayTime;
	}

	void TryExecute(float DeltaSeconds)
	{
		if(!Refresh(DeltaSeconds)) return;

		TaskState = EStepTaskState::Executing;
		RemainTime = DurationTime;

		OnExecuteDelegate.Broadcast();

		WHLog(FString::Printf(TEXT("ExecuteStepTask : TaskName = %s"), *TaskName.ToString()), EDC_Step);
	}

	void TryComplete(float DeltaSeconds)
	{
		if(!OnCheckDelegate.IsBound())
		{
			if(Refresh(DeltaSeconds))
			{
				TaskState = EStepTaskState::Completed;
			}
		}
		else if(OnCheckDelegate.Execute())
		{
			TaskState = EStepTaskState::Completed;
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
 * 步骤列表项
 */ 
struct FStepListItem : public TSharedFromThis<FStepListItem>
{
public:
	FStepListItem()
	{
		Step = nullptr;
		ParentListItem = nullptr; 
		SubListItems = TArray<TSharedPtr<FStepListItem>>();
	}

	UStepBase* Step;

	TSharedPtr<FStepListItem> ParentListItem;

	TArray<TSharedPtr<FStepListItem>> SubListItems;

public:
	FStepListItemStates& GetStates() const
	{
		return Step->StepListItemStates;
	}

	int32& GetStepIndex() const
	{
		return Step->StepIndex;
	}

	UStepBase* GetParentStep() const
	{
		return Step->ParentStep;
	}
	
	TArray<UStepBase*>& GetSubSteps()const
	{
		return Step->SubSteps;
	}
	
	void GetSubStepNum(int32& OutNum) const
	{
		OutNum += SubListItems.Num();
		for(auto Iter : SubListItems)
		{
			Iter->GetSubStepNum(OutNum);
		}
	}

	TArray<UStepBase*>& GetParentSubSteps() const
	{
		return ParentListItem->GetSubSteps();
	}
	
	TArray<TSharedPtr<FStepListItem>>& GetParentSubListItems() const
	{
		return ParentListItem->SubListItems;
	}
};
