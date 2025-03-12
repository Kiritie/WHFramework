// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "UObject/Interface.h"
#include "FSMAgentInterface.generated.h"

class UFiniteStateBase;
class UFSMComponent;

// This class does not need to be modified.
UINTERFACE(MinimalAPI, NotBlueprintable)
class UFSMAgentInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API IFSMAgentInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual bool SwitchFiniteState(UFiniteStateBase* InState, const TArray<FParameter>* InParams = nullptr)
	{
		return SwitchFiniteState(InState, InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	virtual bool SwitchFiniteState(UFiniteStateBase* InState, const TArray<FParameter>& InParams);

	virtual bool SwitchFiniteStateByIndex(int32 InStateIndex, const TArray<FParameter>* InParams = nullptr)
	{
		return SwitchFiniteStateByIndex(InStateIndex, InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	virtual bool SwitchFiniteStateByIndex(int32 InStateIndex, const TArray<FParameter>& InParams);

	virtual bool SwitchFiniteStateByName(const FName InStateName, const TArray<FParameter>* InParams = nullptr)
	{
		return SwitchFiniteStateByName(InStateName, InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	virtual bool SwitchFiniteStateByName(const FName InStateName, const TArray<FParameter>& InParams);

	template<class T>
	bool SwitchFiniteStateByClass(const TArray<FParameter>* InParams = nullptr, TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass())
	{
		return SwitchFiniteStateByClass(InStateClass, InParams ? *InParams : TArray<FParameter>());
	}

	template<class T>
	bool SwitchFiniteStateByClass(const TArray<FParameter>& InParams, TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass())
	{
		return SwitchFiniteStateByClass(InStateClass, InParams);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	virtual bool SwitchFiniteStateByClass(TSubclassOf<UFiniteStateBase> InStateClass, const TArray<FParameter>& InParams);

	virtual bool SwitchDefaultFiniteState(const TArray<FParameter>* InParams = nullptr)
	{
		return SwitchDefaultFiniteState(InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	virtual bool SwitchDefaultFiniteState(const TArray<FParameter>& InParams);

	virtual bool SwitchFinalFiniteState(const TArray<FParameter>* InParams = nullptr)
	{
		return SwitchFinalFiniteState(InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	virtual bool SwitchFinalFiniteState(const TArray<FParameter>& InParams);

	UFUNCTION(BlueprintCallable)
	virtual void RefreshFiniteState();

	UFUNCTION(BlueprintCallable)
	virtual bool TerminateFiniteState(UFiniteStateBase* InState);

public:
	virtual void OnFiniteStateRefresh(UFiniteStateBase* InCurrentState) { }

	virtual void OnFiniteStateChanged(UFiniteStateBase* InCurrentState, UFiniteStateBase* InLastState) { }

public:
	virtual UFSMComponent* GetFSMComponent() const = 0;
	
	UFUNCTION(BlueprintCallable)
	virtual bool HasFiniteState(UFiniteStateBase* InState) const;

	UFUNCTION(BlueprintCallable)
	virtual bool HasFiniteStateByIndex(int32 InStateIndex) const;

	template<class T>
	T* GetFiniteStateByIndex(int32 InStateIndex, TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass()) const
	{
		return Cast<T>(GetFiniteStateByIndex(InStateIndex, InStateClass));
	}
	
	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"))
	virtual UFiniteStateBase* GetFiniteStateByIndex(int32 InStateIndex, TSubclassOf<UFiniteStateBase> InStateClass) const;
	
	UFUNCTION(BlueprintCallable)
	virtual bool HasFiniteStateByName(const FName InStateName) const;

	template<class T>
	T* GetFiniteStateByName(const FName InStateName, TSubclassOf<UFiniteStateBase> InStateClass = nullptr) const
	{
		return Cast<T>(GetFiniteStateByName(InStateName, InStateClass));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"))
	virtual UFiniteStateBase* GetFiniteStateByName(const FName InStateName, TSubclassOf<UFiniteStateBase> InStateClass) const;

	template<class T>
	bool HasFiniteStateByClass(TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass()) const
	{
		return HasFiniteStateByClass(InStateClass);
	}
	
	UFUNCTION(BlueprintCallable)
	virtual bool HasFiniteStateByClass(TSubclassOf<UFiniteStateBase> InStateClass) const;

	template<class T>
	T* GetFiniteStateByClass(TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass()) const
	{
		return Cast<T>(GetFiniteStateByClass(InStateClass));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"))
	virtual UFiniteStateBase* GetFiniteStateByClass(TSubclassOf<UFiniteStateBase> InStateClass) const;

	UFUNCTION(BlueprintCallable)
	virtual bool IsCurrentFiniteState(UFiniteStateBase* InState) const;

	UFUNCTION(BlueprintCallable)
	virtual bool IsCurrentFiniteStateIndex(int32 InStateIndex) const;

	template<class T>
	bool IsCurrentFiniteStateClass(TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass()) const
	{
		return IsCurrentFiniteStateClass(InStateClass);
	}

	UFUNCTION(BlueprintCallable)
	virtual bool IsCurrentFiniteStateClass(TSubclassOf<UFiniteStateBase> InStateClass) const;

	template<class T>
	T* GetCurrentFiniteState() const
	{
		return Cast<T>(GetCurrentFiniteState());
	}
	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"))
	virtual UFiniteStateBase* GetCurrentFiniteState(TSubclassOf<UFiniteStateBase> InClass = nullptr) const;
};
