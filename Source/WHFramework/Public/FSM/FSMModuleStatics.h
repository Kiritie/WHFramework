// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "FSMModuleTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"

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
	UFUNCTION(BlueprintCallable, Category = "FSMModuleStatics")
	static void SwitchGroupStateByIndex(const FName InGroupName, int32 InStateIndex);

	template<class T>
	static void SwitchGroupStateByClass(const FName InGroupName) { SwitchGroupStateByClass(InGroupName, T::StaticClass()); }

	UFUNCTION(BlueprintCallable, Category = "FSMModuleStatics")
	static void SwitchGroupStateByClass(const FName InGroupName, TSubclassOf<UFiniteStateBase> InStateClass);

	UFUNCTION(BlueprintCallable, Category = "FSMModuleStatics")
	static void SwitchGroupDefaultState(const FName InGroupName);

	UFUNCTION(BlueprintCallable, Category = "FSMModuleStatics")
	static void SwitchGroupFinalState(const FName InGroupName);

	UFUNCTION(BlueprintCallable, Category = "FSMModuleStatics")
	static void SwitchGroupLastState(const FName InGroupName);

	UFUNCTION(BlueprintCallable, Category = "FSMModuleStatics")
	static void SwitchGroupNextState(const FName InGroupName);
};
