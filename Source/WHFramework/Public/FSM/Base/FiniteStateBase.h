// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "FiniteStateBase.generated.h"

class UFSMComponent;

/**
 * 状态基类
 */
UCLASS(Blueprintable, meta = (ShowWorldContextPin), hidecategories = (Default, Tick, Replication, Rendering, Collision, Actor, Input, LOD, Cooking, Hidden, WorldPartition, Hlod, DataLayers))
class WHFRAMEWORK_API UFiniteStateBase : public UObject
{
	GENERATED_BODY()

public:
	UFiniteStateBase();

public:
	/// 状态名称
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName StateName;
	/// 状态索引
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 StateIndex;
	/// 状态机组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UFSMComponent* FSMComponent;

public:
	/**
	 * 状态初始化
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnInitialize")
	void K2_OnInitialize(UFSMComponent* InFSMComponent);
	virtual void OnInitialize(UFSMComponent* InFSMComponent);
	/**
	 * 状态进入
	 * @param InLastFiniteState 上一个状态
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnEnter")
	void K2_OnEnter(UFiniteStateBase* InLastFiniteState);
	virtual void OnEnter(UFiniteStateBase* InLastFiniteState);
	/**
	 * 状态帧刷新
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRefresh")
	void K2_OnRefresh();
	virtual void OnRefresh();
	/**
	 * 状态离开
	 * @param InNextFiniteState 下一个状态
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnLeave")
	void K2_OnLeave(UFiniteStateBase* InNextFiniteState);
	virtual void OnLeave(UFiniteStateBase* InNextFiniteState);
	/**
	 * 状态终止
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnTerminate")
	void K2_OnTerminate();
	virtual void OnTerminate();

public:
	/**
	* 切换到上一个状态
	*/
	UFUNCTION(BlueprintCallable)
	void SwitchLastState();
	/**
	* 切换到下一个状态
	*/
	UFUNCTION(BlueprintCallable)
	void SwitchNextState();
};
