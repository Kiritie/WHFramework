// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Manager/EventManagerBase.h"

#include "Event/EventModuleStatics.h"
#include "Event/Handle/Common/EventHandle_StartGame.h"
#include "Event/Handle/Common/EventHandle_ExitGame.h"
#include "Event/Handle/Common/EventHandle_InitGame.h"

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
	
	UEventModuleStatics::SubscribeEvent<UEventHandle_InitGame>(this, FName("OnInitGame"));
	UEventModuleStatics::SubscribeEvent<UEventHandle_StartGame>(this, FName("OnStartGame"));
	UEventModuleStatics::SubscribeEvent<UEventHandle_ExitGame>(this, FName("OnExitGame"));
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

void UEventManagerBase::OnInitGame(UObject* InSender, UEventHandle_InitGame* InEventHandle)
{
	K2_OnInitGame(InSender, InEventHandle);
}

void UEventManagerBase::OnStartGame(UObject* InSender, UEventHandle_StartGame* InEventHandle)
{
	K2_OnStartGame(InSender, InEventHandle);
}

void UEventManagerBase::OnExitGame(UObject* InSender, UEventHandle_ExitGame* InEventHandle)
{
	K2_OnExitGame(InSender, InEventHandle);
}
