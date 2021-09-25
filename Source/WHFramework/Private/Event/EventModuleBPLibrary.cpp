// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/EventModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Event/EventModule.h"
#include "Event/EventModuleNetworkComponent.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

AEventModule* UEventModuleBPLibrary::EventModuleInst = nullptr;

AEventModule* UEventModuleBPLibrary::GetEventModule(const UObject* InWorldContext)
{
	if (!EventModuleInst || !EventModuleInst->IsValidLowLevel())
	{
		if(AMainModule* MainModule = UMainModuleBPLibrary::GetMainModule(InWorldContext))
		{
			EventModuleInst = MainModule->GetModuleByClass<AEventModule>();
		}
	}
	return EventModuleInst;
}

UEventModuleNetworkComponent* UEventModuleBPLibrary::GetEventModuleNetworkComponent(UObject* InWorldContext)
{
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(InWorldContext, 0))
	{
		return Cast<UEventModuleNetworkComponent>(PlayerController->FindComponentByClass(UEventModuleNetworkComponent::StaticClass()));
	}
	return nullptr;
}

void UEventModuleBPLibrary::SubscribeEvent(const UObject* InWorldContext, TSubclassOf<UEventHandleBase> InEventHandleClass, UObject* InOwner, const FName InFuncName)
{
	if(AEventModule* EventModule = GetEventModule(InWorldContext))
	{
		EventModule->SubscribeEvent(InEventHandleClass, InOwner, InFuncName);
	}
}

void UEventModuleBPLibrary::UnsubscribeEvent(const UObject* InWorldContext, TSubclassOf<UEventHandleBase> InEventHandleClass, UObject* InOwner, const FName InFuncName)
{
	if(AEventModule* EventModule = GetEventModule(InWorldContext))
	{
		EventModule->UnsubscribeEvent(InEventHandleClass, InOwner, InFuncName);
	}
}

void UEventModuleBPLibrary::UnsubscribeAllEvent(const UObject* InWorldContext)
{
	if(AEventModule* EventModule = GetEventModule(InWorldContext))
	{
		EventModule->UnsubscribeAllEvent();
	}
}

void UEventModuleBPLibrary::BroadcastEvent(const UObject* InWorldContext, TSubclassOf<UEventHandleBase> InEventHandleClass, EEventNetType InEventNetType, UObject* InSender, const TArray<FParameter>& InParameters)
{
	if(AEventModule* EventModule = GetEventModule(InWorldContext))
	{
		EventModule->BroadcastEvent(InEventHandleClass, InEventNetType, InSender, InParameters);
	}
}
