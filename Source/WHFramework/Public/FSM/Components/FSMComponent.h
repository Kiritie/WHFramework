// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FSM/FSMModuleTypes.h"
#include "FSM/Base/FiniteStateBase.h"
#include "FSM/Base/FSMAgentInterface.h"

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
	UFUNCTION(BlueprintCallable)
	bool SwitchState(UFiniteStateBase* InState);

	UFUNCTION(BlueprintCallable)
	bool SwitchStateByIndex(int32 InStateIndex);

	template<class T>
	bool SwitchStateByClass() { return SwitchStateByClass(T::StaticClass()); }

	UFUNCTION(BlueprintCallable)
	bool SwitchStateByClass(TSubclassOf<UFiniteStateBase> InStateClass);

	UFUNCTION(BlueprintCallable)
	bool SwitchDefaultState();

	UFUNCTION(BlueprintCallable)
	bool SwitchFinalState();

	UFUNCTION(BlueprintCallable)
	bool SwitchLastState();

	UFUNCTION(BlueprintCallable)
	bool SwitchNextState();

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
	T* GetStateByIndex(int32 InStateIndex) const
	{
		if(HasStateByIndex(InStateIndex))
		{
			return Cast<T>(GetStates()[InStateIndex]);
		}
		return nullptr;
	}
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InStateIndex"))
	UFiniteStateBase* GetStateByIndex(int32 InStateIndex, TSubclassOf<UFiniteStateBase> InStateClass) const;
	
	template<class T>
	bool HasStateByClass(TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass()) const
	{
		if(!InStateClass) return false;

		const FName StateName = InStateClass->GetDefaultObject<UFiniteStateBase>()->GetStateName();
		return StateMap.Contains(StateName);
	}
	
	UFUNCTION(BlueprintPure)
	bool HasStateByClass(TSubclassOf<UFiniteStateBase> InStateClass) const;

	template<class T>
	T* GetStateByClass(TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass()) const
	{
		if(!InStateClass) return nullptr;

		if(HasStateByClass<T>(InStateClass))
		{
			const FName StateName = InStateClass->GetDefaultObject<UFiniteStateBase>()->GetStateName();
			return Cast<T>(StateMap[StateName]);
		}
		return nullptr;
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
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InStateClass"))
	UFiniteStateBase* GetCurrentState(TSubclassOf<UFiniteStateBase> InStateClass = nullptr) const { return CurrentState; }
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

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InAgentClass"))
	AActor* GetAgent(TSubclassOf<AActor> InAgentClass = nullptr) const { return GetOwner(); }
};
