// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Manager/EventManagerBase.h"

#include "Event/EventModuleStatics.h"
#include "Event/Handle/Common/Game/EventHandle_GameStarted.h"
#include "Event/Handle/Common/Game/EventHandle_GameExited.h"
#include "Event/Handle/Common/Game/EventHandle_GameInited.h"

// ParamSets default values
UEventManagerBase::UEventManagerBase()
{
	EventHandleClasses = TArray<TSubclassOf<UEventHandleBase>>();
}

void UEventManagerBase::OnInitialize()
{
	K2_OnInitialize();

	for(const auto Iter : EventHandleClasses)
	{
		if(Iter) UEventModuleStatics::SubscribeEvent(Iter, this, FName("OnHandleEvent"));
	}
	
	UEventModuleStatics::SubscribeEvent<UEventHandle_GameInited>(this, FName("OnGameInited"));
	UEventModuleStatics::SubscribeEvent<UEventHandle_GameStarted>(this, FName("OnGameStarted"));
	UEventModuleStatics::SubscribeEvent<UEventHandle_GameExited>(this, FName("OnGameExited"));
}

void UEventManagerBase::OnPreparatory(EPhase InPhase)
{
	K2_OnPreparatory(InPhase);
}

void UEventManagerBase::OnRefresh(float DeltaSeconds)
{
	K2_OnRefresh(DeltaSeconds);
}

void UEventManagerBase::OnTermination(EPhase InPhase)
{
	K2_OnTermination(InPhase);
}

void UEventManagerBase::OnHandleEvent(UObject* InSender, UEventHandleBase* InEventHandle)
{
	K2_OnHandleEvent(InSender, InEventHandle);
}

void UEventManagerBase::OnGameInited(UObject* InSender, UEventHandle_GameInited* InEventHandle)
{
	K2_OnGameInited(InSender, InEventHandle);
}

void UEventManagerBase::OnGameStarted(UObject* InSender, UEventHandle_GameStarted* InEventHandle)
{
	K2_OnGameStarted(InSender, InEventHandle);
}

void UEventManagerBase::OnGameExited(UObject* InSender, UEventHandle_GameExited* InEventHandle)
{
	K2_OnGameExited(InSender, InEventHandle);
}
