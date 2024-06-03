// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/CommonTypes.h"
#include "FSM/FSMModuleTypes.h"
#include "FSM/Base/FiniteStateBase.h"

#include "FSMComponent.generated.h"

class UFiniteStateBase;

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UFSMComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ParamSets default values for this actor's properties
	UFSMComponent();

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	//////////////////////////////////////////////////////////////////////////
	/// FSM
public:
	UFUNCTION(BlueprintCallable)
	void OnInitialize();

	UFUNCTION(BlueprintCallable)
	void OnRefresh();

	UFUNCTION(BlueprintCallable)
	void OnTermination();

	//////////////////////////////////////////////////////////////////////////
	/// State
public:
	bool SwitchState(UFiniteStateBase* InState, const TArray<FParameter>* InParams = nullptr)
	{
		return SwitchState(InState, InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	bool SwitchState(UFiniteStateBase* InState, const TArray<FParameter>& InParams);

	bool SwitchStateByIndex(int32 InStateIndex, const TArray<FParameter>* InParams = nullptr)
	{
		return SwitchStateByIndex(InStateIndex, InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	bool SwitchStateByIndex(int32 InStateIndex, const TArray<FParameter>& InParams);

	bool SwitchStateByName(const FName InStateName, const TArray<FParameter>* InParams = nullptr)
	{
		return SwitchStateByName(InStateName, InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	bool SwitchStateByName(const FName InStateName, const TArray<FParameter>& InParams);

	template<class T>
	bool SwitchStateByClass(const TArray<FParameter>* InParams = nullptr, TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass())
	{
		return SwitchStateByClass(InStateClass, InParams ? *InParams : TArray<FParameter>());
	}

	template<class T>
	bool SwitchStateByClass(const TArray<FParameter>& InParams, TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass())
	{
		return SwitchStateByClass(InStateClass, InParams);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	bool SwitchStateByClass(TSubclassOf<UFiniteStateBase> InStateClass, const TArray<FParameter>& InParams);

	bool SwitchDefaultState(const TArray<FParameter>* InParams = nullptr)
	{
		return SwitchDefaultState(InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	bool SwitchDefaultState(const TArray<FParameter>& InParams);

	bool SwitchFinalState(const TArray<FParameter>* InParams = nullptr)
	{
		return SwitchFinalState(InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	bool SwitchFinalState(const TArray<FParameter>& InParams);

	bool SwitchLastState(const TArray<FParameter>* InParams = nullptr)
	{
		return SwitchLastState(InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	bool SwitchLastState(const TArray<FParameter>& InParams);

	bool SwitchNextState(const TArray<FParameter>* InParams = nullptr)
	{
		return SwitchNextState(InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	bool SwitchNextState(const TArray<FParameter>& InParams);

	UFUNCTION(BlueprintCallable)
	bool TerminateState(UFiniteStateBase* InState);

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	/// 自动切换默认状态
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	bool bAutoSwitchDefault;
	/// 状态组
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	FName GroupName;
	/// 默认状态
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	TSubclassOf<UFiniteStateBase> DefaultState;
	/// 最终状态
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	TSubclassOf<UFiniteStateBase> FinalState;
	/// 状态列表
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	TArray<TSubclassOf<UFiniteStateBase>> States;
	/// 显示调试信息
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bShowDebugMessage;
	/// 当状态改变
	UPROPERTY(BlueprintAssignable)
	FOnFiniteStateChanged OnStateChanged;

protected:
	/// 当前状态
	UPROPERTY(VisibleAnywhere, Transient, Category = "Stats")
	UFiniteStateBase* CurrentState;
	UPROPERTY()
	TMap<FName, UFiniteStateBase*> StateMap;
	UPROPERTY()
	bool bInitialized;

public:
	UFUNCTION(BlueprintPure)
	bool HasState(UFiniteStateBase* InState) const;

	UFUNCTION(BlueprintPure)
	bool HasStateByIndex(int32 InStateIndex) const;

	template<class T>
	T* GetStateByIndex(int32 InStateIndex, TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass()) const
	{
		return Cast<T>(GetStateByIndex(InStateIndex, InStateClass));
	}
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InStateIndex"))
	UFiniteStateBase* GetStateByIndex(int32 InStateIndex, TSubclassOf<UFiniteStateBase> InStateClass) const;
	
	UFUNCTION(BlueprintPure)
	bool HasStateByName(const FName InStateName) const;

	template<class T>
	T* GetStateByName(const FName InStateName, TSubclassOf<UFiniteStateBase> InStateClass = nullptr) const
	{
		return Cast<T>(GetStateByName(InStateName, InStateClass));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InStateClass"))
	UFiniteStateBase* GetStateByName(const FName InStateName, TSubclassOf<UFiniteStateBase> InStateClass) const;

	template<class T>
	bool HasStateByClass(TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass()) const
	{
		return HasStateByClass(InStateClass);
	}
	
	UFUNCTION(BlueprintPure)
	bool HasStateByClass(TSubclassOf<UFiniteStateBase> InStateClass) const;

	template<class T>
	T* GetStateByClass(TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass()) const
	{
		return Cast<T>(GetStateByClass(InStateClass));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InStateClass"))
	UFiniteStateBase* GetStateByClass(TSubclassOf<UFiniteStateBase> InStateClass) const;

	UFUNCTION(BlueprintPure)
	bool IsCurrentState(UFiniteStateBase* InState) const;

	UFUNCTION(BlueprintPure)
	bool IsCurrentStateIndex(int32 InStateIndex) const;

	template<class T>
	bool IsCurrentStateClass(TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass()) const
	{
		return IsCurrentStateClass(InStateClass);
	}

	UFUNCTION(BlueprintPure)
	bool IsCurrentStateClass(TSubclassOf<UFiniteStateBase> InStateClass) const
	{
		if(!InStateClass) return false;

		const FName StateName = InStateClass->GetDefaultObject<UFiniteStateBase>()->GetStateName();
		return CurrentState && CurrentState->GetStateName() == StateName;
	}
	/**
	* 获取当前状态
	*/
	template<class T>
	T* GetCurrentState() const
	{
		return Cast<T>(CurrentState);
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UFiniteStateBase* GetCurrentState(TSubclassOf<UFiniteStateBase> InClass = nullptr) const { return GetDeterminesOutputObject(CurrentState, InClass); }
	/**
	* 获取状态列表
	*/
	UFUNCTION(BlueprintPure)
	TArray<UFiniteStateBase*>& GetStates() const
	{
		static TArray<UFiniteStateBase*> TempStates;
		StateMap.GenerateValueArray(TempStates);
		return TempStates;
	}
	
	UFUNCTION(BlueprintPure)
	TMap<FName, UFiniteStateBase*>& GetStateMap() { return StateMap; }

	template<class T>
	T* GetAgent() const
	{
		return Cast<T>(GetAgent());
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	AActor* GetAgent(TSubclassOf<AActor> InClass = nullptr) const { return GetDeterminesOutputObject(GetOwner(), InClass); }
};
