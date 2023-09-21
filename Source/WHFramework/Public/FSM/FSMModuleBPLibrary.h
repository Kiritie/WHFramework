// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "FSMModuleTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "FSMModuleBPLibrary.generated.h"

class UFSMBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UFSMModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
	//////////////////////////////////////////////////////////////////////////
	/// FSM
public:
	UFUNCTION(BlueprintCallable, Category = "FSMModuleBPLibrary")
	static void RegisterFSM(UFSMComponent* InFSM);

	UFUNCTION(BlueprintCallable, Category = "FSMModuleBPLibrary")
	static void UnregisterFSM(UFSMComponent* InFSM);

	//////////////////////////////////////////////////////////////////////////
	/// FSMGroup
public:
	UFUNCTION(BlueprintCallable, Category = "FSMModuleBPLibrary")
	static void SwitchGroupStateByIndex(const FName InGroupName, int32 InStateIndex);

	template<class T>
	static void SwitchGroupStateByClass(const FName InGroupName) { SwitchGroupStateByClass(InGroupName, T::StaticClass()); }

	UFUNCTION(BlueprintCallable, Category = "FSMModuleBPLibrary")
	static void SwitchGroupStateByClass(const FName InGroupName, TSubclassOf<UFiniteStateBase> InStateClass);

	UFUNCTION(BlueprintCallable, Category = "FSMModuleBPLibrary")
	static void SwitchGroupDefaultState(const FName InGroupName);

	UFUNCTION(BlueprintCallable, Category = "FSMModuleBPLibrary")
	static void SwitchGroupFinalState(const FName InGroupName);

	UFUNCTION(BlueprintCallable, Category = "FSMModuleBPLibrary")
	static void SwitchGroupLastState(const FName InGroupName);

	UFUNCTION(BlueprintCallable, Category = "FSMModuleBPLibrary")
	static void SwitchGroupNextState(const FName InGroupName);
};
