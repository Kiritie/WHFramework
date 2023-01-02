// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "Main/Base/ModuleNetworkComponent.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventModuleNetworkComponent.generated.h"

class UEventHandleBase;
/**
* 
*/
UCLASS()
class WHFRAMEWORK_API UEventModuleNetworkComponent : public UModuleNetworkComponent
{
	GENERATED_BODY()

public:
	UEventModuleNetworkComponent();
	
public:
	UFUNCTION(Client, Reliable, WithValidation)
	void ClientBroadcastEvent(UObject* InSender, TSubclassOf<UEventHandleBase> InEventHandleClass, const TArray<FParameter>& InParams);
	
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerBroadcastEvent(UObject* InSender, TSubclassOf<UEventHandleBase> InEventHandleClass, const TArray<FParameter>& InParams);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerBroadcastEventMulticast(UObject* InSender, TSubclassOf<UEventHandleBase> InEventHandleClass, const TArray<FParameter>& InParams);
};
