// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Main/Base/ModuleNetworkComponent.h"
#include "Parameter/ParameterModuleTypes.h"

#include "NetworkModuleNetworkComponent.generated.h"

/**
* 
*/
UCLASS()
class WHFRAMEWORK_API UNetworkModuleNetworkComponent : public UModuleNetworkComponent
{
	GENERATED_BODY()

public:
	UNetworkModuleNetworkComponent();

public:
	UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable)
	void ServerExecuteActorFunc(AActor* InActor, FName InFuncName);

	UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable)
	void ServerExecuteActorFuncOneParam(AActor* InActor, FName InFuncName, FParameter InParam);

	UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable)
	void ServerExecuteActorFuncTwoParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2);

	UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable)
	void ServerExecuteActorFuncThreeParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3);

	UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable)
	void ServerExecuteActorFuncFourParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4);

	UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable)
	void ServerExecuteActorFuncFiveParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4, FParameter InParam5);
};
