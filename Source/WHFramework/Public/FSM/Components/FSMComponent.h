// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	//////////////////////////////////////////////////////////////////////////
	/// State
public:
	UFUNCTION(BlueprintCallable)
	void SwitchState(UFiniteStateBase* InState);

	UFUNCTION(BlueprintCallable)
	void SwitchStateByIndex(int32 InStateIndex);

	template<class T>
	void SwitchStateByClass(TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass())
	{
		SwitchStateByClass(InStateClass);
	}

	UFUNCTION(BlueprintCallable)
	void SwitchStateByClass(TSubclassOf<UFiniteStateBase> InStateClass);

	UFUNCTION(BlueprintCallable)
	void SwitchDefaultState();

	UFUNCTION(BlueprintCallable)
	void SwitchFinalState();

	UFUNCTION(BlueprintCallable)
	void SwitchLastState();

	UFUNCTION(BlueprintCallable)
	void SwitchNextState();

	UFUNCTION(BlueprintCallable)
	void ClearAllState();

	//////////////////////////////////////////////////////////////////////////
	/// State Stats
protected:
	/// 状态列表
	UPROPERTY(VisibleAnywhere, Category = "State Stats")
	TArray<TSubclassOf<UFiniteStateBase>> States;
	/// 初始状态
	UPROPERTY(VisibleAnywhere, Category = "State Stats")
	TSubclassOf<UFiniteStateBase> DefaultState;
	/// 最终状态
	UPROPERTY(VisibleAnywhere, Category = "State Stats")
	TSubclassOf<UFiniteStateBase> FinalState;
	/// 当前状态
	UPROPERTY(VisibleAnywhere, Transient, Category = "State Stats")
	UFiniteStateBase* CurrentState;
	UPROPERTY()
	TMap<FName, UFiniteStateBase*> StateMap;

public:
	UFUNCTION(BlueprintPure)
	bool HasStateByIndex(int32 InStateIndex) const;

	template<class T>
	T* GetStateByIndex(int32 InStateIndex) const
	{
		if(!InStateIndex) return nullptr;

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

		const FName StateName = InStateClass->GetDefaultObject<UFiniteStateBase>()->StateName;
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
			const FName StateName = InStateClass->GetDefaultObject<UFiniteStateBase>()->StateName;
			return Cast<T>(StateMap[StateName]);
		}
		return nullptr;
	}
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InStateClass"))
	UFiniteStateBase* GetStateByClass(TSubclassOf<UFiniteStateBase> InStateClass) const;
	
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

		const FName StateName = InStateClass->GetDefaultObject<UFiniteStateBase>()->StateName;
		return CurrentState && CurrentState->StateName == StateName;
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
		TArray<UFiniteStateBase*> TempStates;
		StateMap.GenerateValueArray(TempStates);
		return TempStates;
	}
	
	UFUNCTION(BlueprintPure)
	TMap<FName, UFiniteStateBase*>& GetStateMap() { return StateMap; }
};
