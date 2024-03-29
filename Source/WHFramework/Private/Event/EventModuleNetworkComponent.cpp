// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/EventModuleNetworkComponent.h"

#include "Event/EventModule.h"
#include "Event/EventModuleBPLibrary.h"

UEventModuleNetworkComponent::UEventModuleNetworkComponent()
{
	
}

bool UEventModuleNetworkComponent::ClientBroadcastEvent_Validate(UObject* InSender, TSubclassOf<UEventHandleBase> InClass, const TArray<FParameter>& InParams) { return true; }
void UEventModuleNetworkComponent::ClientBroadcastEvent_Implementation(UObject* InSender, TSubclassOf<UEventHandleBase> InClass, const TArray<FParameter>& InParams)
{
	if(AEventModule* EventModule = AEventModule::Get())
	{
		EventModule->BroadcastEvent(InClass, EEventNetType::Single, InSender, InParams);
	}
}

bool UEventModuleNetworkComponent::ServerBroadcastEvent_Validate(UObject* InSender, TSubclassOf<UEventHandleBase> InClass, const TArray<FParameter>& InParams) { return true; }
void UEventModuleNetworkComponent::ServerBroadcastEvent_Implementation(UObject* InSender, TSubclassOf<UEventHandleBase> InClass, const TArray<FParameter>& InParams)
{
	if(AEventModule* EventModule = AEventModule::Get())
	{
		EventModule->BroadcastEvent(InClass, EEventNetType::Single, InSender, InParams);
	}
}

bool UEventModuleNetworkComponent::ServerBroadcastEventMulticast_Validate(UObject* InSender, TSubclassOf<UEventHandleBase> InClass, const TArray<FParameter>& InParams) { return true; }
void UEventModuleNetworkComponent::ServerBroadcastEventMulticast_Implementation(UObject* InSender, TSubclassOf<UEventHandleBase> InClass, const TArray<FParameter>& InParams)
{
	if(AEventModule* EventModule = AEventModule::Get())
	{
		EventModule->MultiBroadcastEvent(InClass, InSender, InParams);
	}
}
