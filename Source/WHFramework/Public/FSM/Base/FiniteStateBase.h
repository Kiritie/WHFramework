// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHObject.h"

#include "FiniteStateBase.generated.h"

class UFSMComponent;

/**
 * 状态基类
 */
UCLASS(Blueprintable, hidecategories = (Default, Tick, Replication, Rendering, Collision, Actor, Input, LOD, Cooking, Hidden, WorldPartition, Hlod, DataLayers))
class WHFRAMEWORK_API UFiniteStateBase : public UWHObject
{
	GENERATED_BODY()

public:
	UFiniteStateBase();

public:
	virtual int32 GetLimit_Implementation() const override { return 0; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	/**
	 * 状态初始化
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnInitialize")
	void K2_OnInitialize();
	UFUNCTION()
	virtual void OnInitialize(UFSMComponent* InFSM, int32 InStateIndex);
	/**
	 * 状态进入验证
	 * @param InLastState 上一个状态
	 * @param InParams 参数列表
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnEnterValidate")
	bool K2_OnEnterValidate(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams);
	UFUNCTION()
	virtual bool OnEnterValidate(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams);
	/**
	 * 状态进入
	 * @param InLastState 上一个状态
	 * @param InParams 参数列表
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnEnter")
	void K2_OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams);
	UFUNCTION()
	virtual void OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams);
	/**
	 * 状态帧刷新
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRefresh")
	void K2_OnRefresh();
	UFUNCTION()
	virtual void OnRefresh();
	/**
	 * 状态离开验证
	 * @param InNextState 下一个状态
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnLeaveValidate")
	bool K2_OnLeaveValidate(UFiniteStateBase* InNextState);
	UFUNCTION()
	virtual bool OnLeaveValidate(UFiniteStateBase* InNextState);
	/**
	 * 状态离开
	 * @param InNextState 下一个状态
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnLeave")
	void K2_OnLeave(UFiniteStateBase* InNextState);
	UFUNCTION()
	virtual void OnLeave(UFiniteStateBase* InNextState);
	/**
	 * 状态终止
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnTermination")
	void K2_OnTermination();
	UFUNCTION()
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
	bool Switch(UFiniteStateBase* InState, const TArray<FParameter>* InParams = nullptr)
	{
		return Switch(InState, InParams ? *InParams : TArray<FParameter>());
	}
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	bool Switch(UFiniteStateBase* InState, const TArray<FParameter>& InParams);
	/**
	* 切换到上一个状态
	*/
	bool SwitchLast(const TArray<FParameter>* InParams = nullptr)
	{
		return SwitchLast(InParams ? *InParams : TArray<FParameter>());
	}
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	bool SwitchLast(const TArray<FParameter>& InParams);
	/**
	* 切换到下一个状态
	*/
	bool SwitchNext(const TArray<FParameter>* InParams = nullptr)
	{
		return SwitchNext(InParams ? *InParams : TArray<FParameter>());
	}
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	bool SwitchNext(const TArray<FParameter>& InParams);

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
	UFUNCTION(BlueprintPure)
	FName GetStateName() const { return StateName; }
	
	UFUNCTION(BlueprintPure)
	int32 GetStateIndex() const { return StateIndex; }
	
	UFUNCTION(BlueprintCallable)
	void SetStateIndex(int32 InStateIndex) { StateIndex = InStateIndex; }
	
	UFUNCTION(BlueprintPure)
	UFSMComponent* GetFSMComponent() const { return FSM; }

	template<class T>
	T* GetAgent() const
	{
		return Cast<T>(GetAgent());
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	AActor* GetAgent(TSubclassOf<AActor> InClass = nullptr) const;

	UFUNCTION(BlueprintPure)
	bool IsCurrent();
};
