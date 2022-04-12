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

	//////////////////////////////////////////////////////////////////////////
	/// Execute Actor Func
public:
	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void ServerExecuteActorFunc(AActor* InActor, FName InFuncName);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void ServerExecuteActorFuncOneParam(AActor* InActor, FName InFuncName, FParameter InParam);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void ServerExecuteActorFuncTwoParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void ServerExecuteActorFuncThreeParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void ServerExecuteActorFuncFourParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void ServerExecuteActorFuncFiveParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4, FParameter InParam5);
};
