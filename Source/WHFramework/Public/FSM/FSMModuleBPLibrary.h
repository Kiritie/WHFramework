// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FSMModule.h"
#include "FSMModuleTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Main/MainModule.h"

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
	static void SwitchFSMGroupStateByIndex(const FName InFSMGroup, int32 InStateIndex);

	template<class T>
	static void SwitchFSMGroupStateByClass(const FName InFSMGroup) { SwitchFSMGroupStateByClass(T::StaticClass()); }

	UFUNCTION(BlueprintCallable, Category = "FSMModuleBPLibrary")
	static void SwitchFSMGroupStateByClass(const FName InFSMGroup, TSubclassOf<UFiniteStateBase> InStateClass);

	UFUNCTION(BlueprintCallable, Category = "FSMModuleBPLibrary")
	static void SwitchFSMGroupDefaultState(const FName InFSMGroup);

	UFUNCTION(BlueprintCallable, Category = "FSMModuleBPLibrary")
	static void SwitchFSMGroupFinalState(const FName InFSMGroup);

	UFUNCTION(BlueprintCallable, Category = "FSMModuleBPLibrary")
	static void SwitchFSMGroupLastState(const FName InFSMGroup);

	UFUNCTION(BlueprintCallable, Category = "FSMModuleBPLibrary")
	static void SwitchFSMGroupNextState(const FName InFSMGroup);

public:
	UFUNCTION(BlueprintCallable, Category = "FSMModuleBPLibrary")
	static bool HasFSMGroup(const FName InFSMGroup);

	UFUNCTION(BlueprintCallable, Category = "FSMModuleBPLibrary")
	static TArray<UFSMComponent*> GetFSMGroupArray(const FName InFSMGroup);
};
