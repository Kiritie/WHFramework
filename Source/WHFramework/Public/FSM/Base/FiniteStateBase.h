// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
#include "Global/Base/WHObject.h"

#include "FiniteStateBase.generated.h"

class UFSMComponent;

/**
 * 状态基类
 */
UCLASS(Blueprintable, meta = (ShowWorldContextPin), hidecategories = (Default, Tick, Replication, Rendering, Collision, Actor, Input, LOD, Cooking, Hidden, WorldPartition, Hlod, DataLayers))
class WHFRAMEWORK_API UFiniteStateBase : public UWHObject
{
	GENERATED_BODY()

public:
	UFiniteStateBase();

public:
	virtual int32 GetLimit_Implementation() const override { return 0; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	/**
	 * 状态初始化
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnInitialize")
	void K2_OnInitialize();
	virtual void OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex);
	/**
	 * 状态进入验证
	 * @param InLastFiniteState 上一个状态
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnEnterValidate")
	bool K2_OnEnterValidate(UFiniteStateBase* InLastFiniteState);
	virtual bool OnEnterValidate(UFiniteStateBase* InLastFiniteState);
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
	 * 状态离开验证
	 * @param InNextFiniteState 下一个状态
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnLeaveValidate")
	bool K2_OnLeaveValidate(UFiniteStateBase* InNextFiniteState);
	virtual bool OnLeaveValidate(UFiniteStateBase* InNextFiniteState);
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
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnTermination")
	void K2_OnTermination();
	virtual void OnTermination();

public:
	/**
	* 终止状态
	*/
	UFUNCTION(BlueprintCallable)
	void Terminate();
	/**
	* 切换到指定状态
	*/
	UFUNCTION(BlueprintCallable)
	void Switch(UFiniteStateBase* InFiniteState);
	/**
	* 切换到上一个状态
	*/
	UFUNCTION(BlueprintCallable)
	void SwitchLast();
	/**
	* 切换到下一个状态
	*/
	UFUNCTION(BlueprintCallable)
	void SwitchNext();
	/**
	* 是否是当前状态
	*/
	UFUNCTION(BlueprintPure)
	bool IsCurrentState();

protected:
	/// 状态名称
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName StateName;
	/// 状态索引
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 StateIndex;
	/// 状态机组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UFSMComponent* FSM;

private:
	bool bHasBlueprintOnEnterValidate;
	bool bHasBlueprintOnLeaveValidate;
	
public:
	FName GetStateName() const { return StateName; }
	
	int32 GetStateIndex() const { return StateIndex; }
	
	void SetStateIndex(int32 InStateIndex) { this->StateIndex = InStateIndex; }
	
	UFSMComponent* GetFSM() const { return FSM; }

	template<class T>
	T* GetAgent() const
	{
		return Cast<T>(GetAgent());
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InAgentClass"))
	AActor* GetAgent(TSubclassOf<AActor> InAgentClass = nullptr) const;
};
