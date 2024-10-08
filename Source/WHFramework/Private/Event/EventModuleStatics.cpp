// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/EventModuleStatics.h"

#include "Event/EventModule.h"
#include "Event/EventModuleNetworkComponent.h"

void UEventModuleStatics::SubscribeEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InOwner, const FName InFuncName)
{
	UEventModule::Get().SubscribeEvent(InClass, InOwner, InFuncName);
}

void UEventModuleStatics::SubscribeEventByDelegate(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDynamicDelegate& InDelegate)
{
	UEventModule::Get().SubscribeEventByDelegate(InClass, InDelegate);
}

void UEventModuleStatics::UnsubscribeEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InOwner, const FName InFuncName)
{
	UEventModule::Get().UnsubscribeEvent(InClass, InOwner, InFuncName);
}

void UEventModuleStatics::UnsubscribeEventByDelegate(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDynamicDelegate& InDelegate)
{
	UEventModule::Get().UnsubscribeEventByDelegate(InClass, InDelegate);
}

void UEventModuleStatics::UnsubscribeAllEvent()
{
	UEventModule::Get().UnsubscribeAllEvent();
}

void UEventModuleStatics::BroadcastEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InSender, const TArray<FParameter>& InParams, EEventNetType InNetType, bool bRecovery)
{
	UEventModule::Get().BroadcastEvent(InClass, InSender, InParams, InNetType, bRecovery);
}

void UEventModuleStatics::BroadcastEventByHandle(UEventHandleBase* InHandle, UObject* InSender, EEventNetType InNetType, bool bRecovery)
{
	UEventModule::Get().BroadcastEventByHandle(InHandle, InSender, InNetType, bRecovery);
}

UEventManagerBase* UEventModuleStatics::GetEventManager(TSubclassOf<UEventManagerBase> InClass)
{
	return UEventModule::Get().GetEventManager(InClass);
}

UEventManagerBase* UEventModuleStatics::GetEventManagerByName(const FName InName, TSubclassOf<UEventManagerBase> InClass)
{
	return UEventModule::Get().GetEventManagerByName(InName, InClass);
}
