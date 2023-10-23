// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/EventModuleBPLibrary.h"

#include "Event/EventModule.h"
#include "Event/EventModuleNetworkComponent.h"

void UEventModuleBPLibrary::SubscribeEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InOwner, const FName InFuncName)
{
	if(AEventModule* EventModule = AEventModule::Get())
	{
		EventModule->SubscribeEvent(InClass, InOwner, InFuncName);
	}
}

void UEventModuleBPLibrary::SubscribeEvent(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDelegate& InDelegate)
{
	if(AEventModule* EventModule = AEventModule::Get())
	{
		EventModule->SubscribeEvent(InClass, InDelegate);
	}
}

void UEventModuleBPLibrary::SubscribeEventByDelegate(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDynamicDelegate& InDelegate)
{
	if(AEventModule* EventModule = AEventModule::Get())
	{
		EventModule->SubscribeEventByDelegate(InClass, InDelegate);
	}
}

void UEventModuleBPLibrary::UnsubscribeEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InOwner, const FName InFuncName)
{
	if(AEventModule* EventModule = AEventModule::Get())
	{
		EventModule->UnsubscribeEvent(InClass, InOwner, InFuncName);
	}
}

void UEventModuleBPLibrary::UnsubscribeEvent(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDelegate& InDelegate)
{
	if(AEventModule* EventModule = AEventModule::Get())
	{
		EventModule->UnsubscribeEvent(InClass, InDelegate);
	}
}

void UEventModuleBPLibrary::UnsubscribeEventByDelegate(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDynamicDelegate& InDelegate)
{
	if(AEventModule* EventModule = AEventModule::Get())
	{
		EventModule->UnsubscribeEventByDelegate(InClass, InDelegate);
	}
}

void UEventModuleBPLibrary::UnsubscribeAllEvent()
{
	if(AEventModule* EventModule = AEventModule::Get())
	{
		EventModule->UnsubscribeAllEvent();
	}
}

void UEventModuleBPLibrary::BroadcastEvent(TSubclassOf<UEventHandleBase> InClass, EEventNetType InNetType, UObject* InSender, const TArray<FParameter>& InParams)
{
	if(AEventModule* EventModule = AEventModule::Get())
	{
		EventModule->BroadcastEvent(InClass, InNetType, InSender, InParams);
	}
}
