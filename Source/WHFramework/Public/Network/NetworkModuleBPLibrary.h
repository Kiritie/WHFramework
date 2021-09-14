// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Parameter/ParameterModuleTypes.h"

#include "NetworkModuleBPLibrary.generated.h"

class ANetworkModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UNetworkModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static ANetworkModule* NetworkModuleInst;

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "NetworkModuleBPLibrary")
	static ANetworkModule* GetNetworkModule(UObject* InWorldContext);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "NetworkModuleBPLibrary")
	static class UNetworkModuleNetworkComponent* GetNetworkModuleNetworkComponent(UObject* InWorldContext);

	//////////////////////////////////////////////////////////////////////////
	/// Execute Object Func
public:
	static bool ExecuteObjectFunc(UObject* InWorldContext, UObject* InObject, const FName& InFuncName, void* Params);

	//////////////////////////////////////////////////////////////////////////
	/// Execute Actor Func
public:
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "NetworkModuleBPLibrary")
	static void ServerExecuteActorFunc(UObject* InWorldContext, AActor* InActor, FName InFuncName);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "NetworkModuleBPLibrary")
	static void ServerExecuteActorFuncOneParam(UObject* InWorldContext, AActor* InActor, FName InFuncName, FParameter InParam);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "NetworkModuleBPLibrary")
	static void ServerExecuteActorFuncTwoParam(UObject* InWorldContext, AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "NetworkModuleBPLibrary")
	static void ServerExecuteActorFuncThreeParam(UObject* InWorldContext, AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "NetworkModuleBPLibrary")
	static void ServerExecuteActorFuncFourParam(UObject* InWorldContext, AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "NetworkModuleBPLibrary")
	static void ServerExecuteActorFuncFiveParam(UObject* InWorldContext, AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4, FParameter InParam5);
};
