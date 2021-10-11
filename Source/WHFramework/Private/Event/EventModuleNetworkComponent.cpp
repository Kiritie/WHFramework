// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/EventModuleNetworkComponent.h"

#include "MainModule.h"
#include "Event/EventModule.h"
#include "Event/EventModuleBPLibrary.h"

UEventModuleNetworkComponent::UEventModuleNetworkComponent()
{
	
}

bool UEventModuleNetworkComponent::ClientBroadcastEvent_Validate(UObject* InSender, TSubclassOf<UEventHandleBase> InEventHandleClass, const TArray<FParameter>& InParameters) { return true; }
void UEventModuleNetworkComponent::ClientBroadcastEvent_Implementation(UObject* InSender, TSubclassOf<UEventHandleBase> InEventHandleClass, const TArray<FParameter>& InParameters)
{
	if(AEventModule* EventModule = AMainModule::GetModuleByClass<AEventModule>())
	{
		EventModule->BroadcastEvent(InEventHandleClass, EEventNetType::Single, InSender, InParameters);
	}
}

bool UEventModuleNetworkComponent::ServerBroadcastEvent_Validate(UObject* InSender, TSubclassOf<UEventHandleBase> InEventHandleClass, const TArray<FParameter>& InParameters) { return true; }
void UEventModuleNetworkComponent::ServerBroadcastEvent_Implementation(UObject* InSender, TSubclassOf<UEventHandleBase> InEventHandleClass, const TArray<FParameter>& InParameters)
{
	if(AEventModule* EventModule = AMainModule::GetModuleByClass<AEventModule>())
	{
		EventModule->BroadcastEvent(InEventHandleClass, EEventNetType::Single, InSender, InParameters);
	}
}

bool UEventModuleNetworkComponent::ServerBroadcastEventMulticast_Validate(UObject* InSender, TSubclassOf<UEventHandleBase> InEventHandleClass, const TArray<FParameter>& InParameters) { return true; }
void UEventModuleNetworkComponent::ServerBroadcastEventMulticast_Implementation(UObject* InSender, TSubclassOf<UEventHandleBase> InEventHandleClass, const TArray<FParameter>& InParameters)
{
	if(AEventModule* EventModule = AMainModule::GetModuleByClass<AEventModule>())
	{
		EventModule->MultiBroadcastEvent(InEventHandleClass, InSender, InParameters);
	}
}
