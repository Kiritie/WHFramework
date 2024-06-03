// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "FSMModuleTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Parameter/ParameterTypes.h"

#include "FSMModuleStatics.generated.h"

class UFSMBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UFSMModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
	//////////////////////////////////////////////////////////////////////////
	/// FSM
public:
	UFUNCTION(BlueprintCallable, Category = "FSMModuleStatics")
	static void RegisterFSM(UFSMComponent* InFSM);

	UFUNCTION(BlueprintCallable, Category = "FSMModuleStatics")
	static void UnregisterFSM(UFSMComponent* InFSM);

	//////////////////////////////////////////////////////////////////////////
	/// FSMGroup
public:
	UFUNCTION(BlueprintPure, Category = "FSMModuleStatics")
	static bool HasFSMGroup(const FName InGroupName);

	UFUNCTION(BlueprintPure, Category = "FSMModuleStatics")
	static FFSMGroup GetFSMGroup(const FName InGroupName);

	static void SwitchFSMGroupStateByIndex(const FName InGroupName, int32 InStateIndex, const TArray<FParameter>* InParams = nullptr)
	{
		SwitchFSMGroupStateByIndex(InGroupName, InStateIndex, InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "FSMModuleStatics")
	static void SwitchFSMGroupStateByIndex(const FName InGroupName, int32 InStateIndex, const TArray<FParameter>& InParams);

	static void SwitchFSMGroupStateByName(const FName InGroupName, const FName InStateName, const TArray<FParameter>* InParams = nullptr)
	{
		SwitchFSMGroupStateByName(InGroupName, InStateName, InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "FSMModuleStatics")
	static void SwitchFSMGroupStateByName(const FName InGroupName, const FName InStateName, const TArray<FParameter>& InParams);

	template<class T>
	static void SwitchFSMGroupStateByClass(const FName InGroupName, const TArray<FParameter>* InParams = nullptr, TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass())
	{
		SwitchFSMGroupStateByClass(InGroupName, InStateClass, InParams ? *InParams : TArray<FParameter>());
	}

	template<class T>
	static void SwitchFSMGroupStateByClass(const FName InGroupName, const TArray<FParameter>& InParams, TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass())
	{
		SwitchFSMGroupStateByClass(InGroupName, InStateClass, InParams);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "FSMModuleStatics")
	static void SwitchFSMGroupStateByClass(const FName InGroupName, TSubclassOf<UFiniteStateBase> InStateClass, const TArray<FParameter>& InParams);

	static void SwitchFSMGroupDefaultState(const FName InGroupName, const TArray<FParameter>* InParams = nullptr)
	{
		SwitchFSMGroupDefaultState(InGroupName, InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "FSMModuleStatics")
	static void SwitchFSMGroupDefaultState(const FName InGroupName, const TArray<FParameter>& InParams);

	static void SwitchFSMGroupFinalState(const FName InGroupName, const TArray<FParameter>* InParams = nullptr)
	{
		SwitchFSMGroupFinalState(InGroupName, InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "FSMModuleStatics")
	static void SwitchFSMGroupFinalState(const FName InGroupName, const TArray<FParameter>& InParams);

	static void SwitchFSMGroupLastState(const FName InGroupName, const TArray<FParameter>* InParams = nullptr)
	{
		SwitchFSMGroupLastState(InGroupName, InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "FSMModuleStatics")
	static void SwitchFSMGroupLastState(const FName InGroupName, const TArray<FParameter>& InParams);

	static void SwitchFSMGroupNextState(const FName InGroupName, const TArray<FParameter>* InParams = nullptr)
	{
		SwitchFSMGroupNextState(InGroupName, InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "FSMModuleStatics")
	static void SwitchFSMGroupNextState(const FName InGroupName, const TArray<FParameter>& InParams);
};
