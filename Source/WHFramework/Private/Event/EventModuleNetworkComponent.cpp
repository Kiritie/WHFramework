// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/EventModuleNetworkComponent.h"

#include "Event/EventModule.h"
#include "Event/EventModuleStatics.h"

UEventModuleNetworkComponent::UEventModuleNetworkComponent()
{
	
}

bool UEventModuleNetworkComponent::ClientBroadcastEvent_Validate(UObject* InSender, TSubclassOf<UEventHandleBase> InClass, const TArray<FParameter>& InParams) { return true; }
void UEventModuleNetworkComponent::ClientBroadcastEvent_Implementation(UObject* InSender, TSubclassOf<UEventHandleBase> InClass, const TArray<FParameter>& InParams)
{
	UEventModule::Get().BroadcastEvent(InClass, InSender, InParams, EEventNetType::Single);
}

bool UEventModuleNetworkComponent::ServerBroadcastEvent_Validate(UObject* InSender, TSubclassOf<UEventHandleBase> InClass, const TArray<FParameter>& InParams, bool bRecovery) { return true; }
void UEventModuleNetworkComponent::ServerBroadcastEvent_Implementation(UObject* InSender, TSubclassOf<UEventHandleBase> InClass, const TArray<FParameter>& InParams, bool bRecovery)
{
	UEventModule::Get().BroadcastEvent(InClass, InSender, InParams, EEventNetType::Single, bRecovery);
}

bool UEventModuleNetworkComponent::ServerBroadcastEventMulticast_Validate(UObject* InSender, TSubclassOf<UEventHandleBase> InClass, const TArray<FParameter>& InParams, bool bRecovery) { return true; }
void UEventModuleNetworkComponent::ServerBroadcastEventMulticast_Implementation(UObject* InSender, TSubclassOf<UEventHandleBase> InClass, const TArray<FParameter>& InParams, bool bRecovery)
{
	UEventModule::Get().MultiBroadcastEvent(InClass, InSender, InParams, bRecovery);
}
