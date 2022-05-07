// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Procedure/ProcedureModuleTypes.h"
#include "Debug/DebugModuleTypes.h"
#include "Math/MathTypes.h"

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
	 * 流程构建
	 */
	virtual void OnGenerate();
	/**
	 * 流程取消构建
	 */
	virtual void OnUnGenerate();
	/**
	 * 流程复制
	 */
	virtual void OnDuplicate(UProcedureBase* InNewProcedure);
#endif

public:
	/**
	* 流程状态改变
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnStateChanged")
	void K2_OnStateChanged(EProcedureState InProcedureState);
	virtual void OnStateChanged(EProcedureState InProcedureState);
	/**
	 * 流程初始化
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnInitialize")
	void K2_OnInitialize();
	virtual void OnInitialize();
	/**
	 * 流程还原
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRestore")
	void K2_OnRestore();
	virtual void OnRestore();
	/**
	 * 流程进入
	 * @param InLastProcedure 上一个流程
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnEnter")
	void K2_OnEnter(UProcedureBase* InLastProcedure);
	virtual void OnEnter(UProcedureBase* InLastProcedure);
	/**
	 * 流程帧刷新
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRefresh")
	void K2_OnRefresh();
	virtual void OnRefresh();
	/**
	* 流程指引
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnGuide")
	void K2_OnGuide();
	virtual void OnGuide();
	/**
	 * 流程执行
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnExecute")
	void K2_OnExecute();
	virtual void OnExecute();
	/**
	 * 流程完成
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnComplete")
	void K2_OnComplete(EProcedureExecuteResult InProcedureExecuteResult);
	virtual void OnComplete(EProcedureExecuteResult InProcedureExecuteResult);
	/**
	 * 流程离开
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnLeave")
	void K2_OnLeave();
	virtual void OnLeave();

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
	* 进入流程
	*/
	UFUNCTION(BlueprintCallable)
	void Refresh();
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
	/// Name/Description
public:
	/// 流程名称
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Name/Description")
	FName ProcedureName;
	/// 流程显示名称
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Name/Description")
	FText ProcedureDisplayName;
	/// 流程描述
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = "true"), Category = "Name/Description")
	FText ProcedureDescription;

	//////////////////////////////////////////////////////////////////////////
	/// Index/Type/State
public:
	/// 是否为开始流程
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Index/Type/State")
	bool bFirstProcedure;
	/// 流程索引
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Index/Type/State")
	int32 ProcedureIndex;
	/// 流程状态
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Index/Type/State")
	EProcedureState ProcedureState;

	UPROPERTY(BlueprintAssignable)
	FOnProcedureStateChanged OnProcedureStateChanged;

public:
	/**
	* 获取流程状态
	*/
	UFUNCTION(BlueprintPure)
	EProcedureState GetProcedureState() const { return ProcedureState; }
	/**
	* 是否已进入
	*/
	UFUNCTION(BlueprintPure)
	bool IsEntered() const;
	/**
	* 是否已完成
	*/
	UFUNCTION(BlueprintPure)
	bool IsCompleted() const;

	//////////////////////////////////////////////////////////////////////////
	/// Operation Target
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation Target")
	AActor* OperationTarget;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "OperationTarget != nullptr"), Category = "Operation Target")
	bool bTrackTarget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target|Camera View")
	EProcedureCameraViewMode CameraViewMode;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target|Camera View")
	EProcedureCameraViewSpace CameraViewSpace;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "CameraViewMode == EProcedureCameraViewMode::Duration"), Category = "Operation Target|Camera View")
	EEaseType CameraViewEaseType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "CameraViewMode == EProcedureCameraViewMode::Duration"), Category = "Operation Target|Camera View")
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
#endif
	/**
	* 还原摄像机视角
	*/
	UFUNCTION(BlueprintCallable)
	void ResetCameraView();

	//////////////////////////////////////////////////////////////////////////
	/// Execute/Guide
public:
	/// 流程执行条件
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	EProcedureExecuteResult ProcedureExecuteCondition;
	/// 流程执行结果
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	EProcedureExecuteResult ProcedureExecuteResult;
	/// 流程进入方式
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execute/Guide")
	EProcedureEnterType ProcedureEnterType;
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
	EProcedureEnterType GetProcedureEnterType() const { return ProcedureEnterType; }

	UFUNCTION(BlueprintPure)
	EProcedureExecuteType GetProcedureExecuteType() const { return ProcedureExecuteType; }

	UFUNCTION(BlueprintPure)
	EProcedureLeaveType GetProcedureLeaveType() const { return ProcedureLeaveType; }

	UFUNCTION(BlueprintPure)
	EProcedureCompleteType GetProcedureCompleteType() const { return ProcedureCompleteType; }

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
 * 流程列表项
 */ 
struct FProcedureListItem : public TSharedFromThis<FProcedureListItem>
{
public:
	FProcedureListItem()
	{
		Procedure = nullptr;
	}

	UProcedureBase* Procedure;

public:
	FProcedureListItemStates& GetStates() const
	{
		return Procedure->ProcedureListItemStates;
	}

	int32& GetProcedureIndex() const
	{
		return Procedure->ProcedureIndex;
	}
};
