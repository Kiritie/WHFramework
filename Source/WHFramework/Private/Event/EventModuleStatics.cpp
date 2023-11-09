// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/EventModuleStatics.h"

#include "Event/EventModule.h"
#include "Event/EventModuleNetworkComponent.h"

void UEventModuleStatics::SubscribeEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InOwner, const FName InFuncName)
{
	if(UEventModule* EventModule = UEventModule::Get())
	{
		EventModule->SubscribeEvent(InClass, InOwner, InFuncName);
	}
}

void UEventModuleStatics::SubscribeEvent(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDelegate& InDelegate)
{
	if(UEventModule* EventModule = UEventModule::Get())
	{
		EventModule->SubscribeEvent(InClass, InDelegate);
	}
}

void UEventModuleStatics::SubscribeEventByDelegate(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDynamicDelegate& InDelegate)
{
	if(UEventModule* EventModule = UEventModule::Get())
	{
		EventModule->SubscribeEventByDelegate(InClass, InDelegate);
	}
}

void UEventModuleStatics::UnsubscribeEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InOwner, const FName InFuncName)
{
	if(UEventModule* EventModule = UEventModule::Get())
	{
		EventModule->UnsubscribeEvent(InClass, InOwner, InFuncName);
	}
}

void UEventModuleStatics::UnsubscribeEvent(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDelegate& InDelegate)
{
	if(UEventModule* EventModule = UEventModule::Get())
	{
		EventModule->UnsubscribeEvent(InClass, InDelegate);
	}
}

void UEventModuleStatics::UnsubscribeEventByDelegate(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDynamicDelegate& InDelegate)
{
	if(UEventModule* EventModule = UEventModule::Get())
	{
		EventModule->UnsubscribeEventByDelegate(InClass, InDelegate);
	}
}

void UEventModuleStatics::UnsubscribeAllEvent()
{
	if(UEventModule* EventModule = UEventModule::Get())
	{
		EventModule->UnsubscribeAllEvent();
	}
}

void UEventModuleStatics::BroadcastEvent(TSubclassOf<UEventHandleBase> InClass, EEventNetType InNetType, UObject* InSender, const TArray<FParameter>& InParams)
{
	if(UEventModule* EventModule = UEventModule::Get())
	{
		EventModule->BroadcastEvent(InClass, InNetType, InSender, InParams);
	}
}
