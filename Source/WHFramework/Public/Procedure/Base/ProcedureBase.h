// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Camera/CameraModuleTypes.h"

#include "Procedure/ProcedureModuleTypes.h"
#include "Common/Base/WHObject.h"

#include "ProcedureBase.generated.h"

class UProcedureAsset;
class ACameraActorBase;

#if WITH_EDITORONLY_DATA
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FProcedureListItemStates
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	bool bSelected;

	FORCEINLINE FProcedureListItemStates()
	{
		bSelected = false;
	}
};
#endif

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
	UFUNCTION()
	virtual void OnStateChanged(EProcedureState InProcedureState);
	/**
	 * 流程初始化
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnInitialize")
	void K2_OnInitialize();
	UFUNCTION()
	virtual void OnInitialize();
	/**
	 * 流程进入
	 * @param InLastProcedure 上一个流程
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnEnter")
	void K2_OnEnter(UProcedureBase* InLastProcedure);
	UFUNCTION()
	virtual void OnEnter(UProcedureBase* InLastProcedure);
	/**
	 * 流程帧刷新
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRefresh")
	void K2_OnRefresh();
	UFUNCTION()
	virtual void OnRefresh();
	/**
	* 流程指引
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnGuide")
	void K2_OnGuide();
	UFUNCTION()
	virtual void OnGuide();
	/**
	 * 流程离开
	 * @param InNextProcedure 下一个流程
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnLeave")
	void K2_OnLeave(UProcedureBase* InNextProcedure);
	UFUNCTION()
	virtual void OnLeave(UProcedureBase* InNextProcedure);

public:
	/**
	* 指引流程
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void Guide();
	/**
	* 切入当前流程
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void SwitchIn();
	/**
	* 切出当前流程
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void SwitchOut();
	/**
	* 切换到上一个流程
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void SwitchLast();
	/**
	* 切换到下一个流程
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void SwitchNext();
	/**
	* 是否是当前流程
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditConditionHides, EditCondition = "ProcedureGuideType != EProcedureGuideType::None"), Category = "Index/Type/Guide")
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
	/**
	* 获取流程资产
	*/
	UFUNCTION(BlueprintPure)
	UProcedureAsset* GetProcedureAsset() const;

	//////////////////////////////////////////////////////////////////////////
	/// Operation Target
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation Target")
	TSoftObjectPtr<AActor> OperationTarget;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation Target")
	bool bTrackTarget;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditConditionHides, EditCondition = "bTrackTarget == true"), Category = "Operation Target")
	ECameraTrackMode TrackTargetMode;

	//////////////////////////////////////////////////////////////////////////
	/// Camera View
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation Target|Camera View")
	FCameraViewParams CameraViewParams;

public:
#if WITH_EDITOR
	/**
	* 获取摄像机视角
	*/
	void GetCameraView();
	/**
	* 设置摄像机视角
	*/
	void SetCameraView(const FCameraParams& InCameraParams);
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
	void SetOperationTarget(AActor* InOperationTarget, bool bResetCameraView = true);

	//////////////////////////////////////////////////////////////////////////
	/// ProcedureListItem
public:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FProcedureListItemStates ProcedureListItemStates;
#endif
#if WITH_EDITOR
	/**
	* 构建流程列表项
	*/
	virtual bool GenerateListItem(TSharedPtr<struct FProcedureListItem> OutProcedureListItem, const FString& InFilterText = TEXT(""));
	/**
	* 更新流程列表项
	*/
	virtual void UpdateListItem(TSharedPtr<struct FProcedureListItem> OutProcedureListItem);

	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

/**
 * 流程列表项
 */ 
#if WITH_EDITOR
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
#endif
