// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/EventModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Event/EventModule.h"
#include "Event/EventModuleNetworkComponent.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

void UEventModuleBPLibrary::SubscribeEvent(TSubclassOf<UEventHandleBase> InEventHandleClass, UObject* InOwner, const FName InFuncName)
{
	if(AEventModule* EventModule = AMainModule::GetModuleByClass<AEventModule>())
	{
		EventModule->SubscribeEvent(InEventHandleClass, InOwner, InFuncName);
	}
}

void UEventModuleBPLibrary::UnsubscribeEvent(TSubclassOf<UEventHandleBase> InEventHandleClass, UObject* InOwner, const FName InFuncName)
{
	if(AEventModule* EventModule = AMainModule::GetModuleByClass<AEventModule>())
	{
		EventModule->UnsubscribeEvent(InEventHandleClass, InOwner, InFuncName);
	}
}

void UEventModuleBPLibrary::UnsubscribeAllEvent()
{
	if(AEventModule* EventModule = AMainModule::GetModuleByClass<AEventModule>())
	{
		EventModule->UnsubscribeAllEvent();
	}
}

void UEventModuleBPLibrary::BroadcastEvent(TSubclassOf<UEventHandleBase> InEventHandleClass, EEventNetType InEventNetType, UObject* InSender, const TArray<FParameter>& InParameters)
{
	if(AEventModule* EventModule = AMainModule::GetModuleByClass<AEventModule>())
	{
		EventModule->BroadcastEvent(InEventHandleClass, InEventNetType, InSender, InParameters);
	}
}
