// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Camera/CameraModuleTypes.h"

#include "Procedure/ProcedureModuleTypes.h"
#include "Common/Base/WHObject.h"
#include "Math/MathTypes.h"

#include "ProcedureBase.generated.h"

class ACameraPawnBase;
/**
 * 流程基类
 */
UCLASS(Blueprintable, hidecategories = (Default))
class WHFRAMEWORK_API UProcedureBase : public UWHObject
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
	 * 流程离开
	 * @param InNextProcedure 下一个流程
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnLeave")
	void K2_OnLeave(UProcedureBase* InNextProcedure);
	virtual void OnLeave(UProcedureBase* InNextProcedure);

public:
	/**
	* 指引流程
	*/
	UFUNCTION(BlueprintCallable)
	void Guide();
	/**
	* 切换到上一个流程
	*/
	UFUNCTION(BlueprintCallable)
	void SwitchLast();
	/**
	* 切换到下一个流程
	*/
	UFUNCTION(BlueprintCallable)
	void SwitchNext();
	/**
	* 是否是当前流程
	*/
	UFUNCTION(BlueprintCallable)
	bool IsCurrent();

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
	/// Index/Type/Guide
public:
	/// 是否为开始流程
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Index/Type/Guide")
	bool bFirstProcedure;
	/// 流程索引
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Index/Type/Guide")
	int32 ProcedureIndex;
	/// 流程状态
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Index/Type/Guide")
	EProcedureState ProcedureState;
	/// 流程指引类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Index/Type/Guide")
	EProcedureGuideType ProcedureGuideType;
	/// 流程指引间隔时间 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Index/Type/Guide")
	float ProcedureGuideIntervalTime;

	UPROPERTY(BlueprintAssignable)
	FOnProcedureStateChanged OnProcedureStateChanged;

protected:
	FTimerHandle StartGuideTimerHandle;

public:
	/**
	* 获取流程状态
	*/
	UFUNCTION(BlueprintPure)
	EProcedureState GetProcedureState() const { return ProcedureState; }

	//////////////////////////////////////////////////////////////////////////
	/// Operation Target
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation Target")
	AActor* OperationTarget;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target")
	bool bTrackTarget;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditConditionHides, EditCondition = "bTrackTarget == true"), Category = "Operation Target")
	ETrackTargetMode TrackTargetMode;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target|Camera View")
	ACameraPawnBase* CameraViewPawn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target|Camera View")
	EProcedureCameraViewMode CameraViewMode;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target|Camera View")
	EProcedureCameraViewSpace CameraViewSpace;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditConditionHides, EditCondition = "CameraViewMode == EProcedureCameraViewMode::Duration"), Category = "Operation Target|Camera View")
	EEaseType CameraViewEaseType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditConditionHides, EditCondition = "CameraViewMode == EProcedureCameraViewMode::Duration"), Category = "Operation Target|Camera View")
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
	/**
	* 设置操作目标
	*/
	UFUNCTION(BlueprintCallable)
	void SetOperationTarget(AActor* InOperationTarget, bool bResetCameraView = false);

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
