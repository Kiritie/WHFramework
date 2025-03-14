// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Manager/EventManagerBase.h"

#include "Event/EventModuleStatics.h"
#include "Event/Handle/Common/Game/EventHandle_GameStarted.h"
#include "Event/Handle/Common/Game/EventHandle_GameExited.h"
#include "Event/Handle/Common/Game/EventHandle_GameInited.h"

// ParamSets default values
UEventManagerBase::UEventManagerBase()
{
	EventManagerName = NAME_None;
	
	EventHandleClasses = TArray<TSubclassOf<UEventHandleBase>>();
}

void UEventManagerBase::OnInitialize()
{
	K2_OnInitialize();

	for(const auto Iter : EventHandleClasses)
	{
		if(Iter) UEventModuleStatics::SubscribeEvent(Iter, this, GET_FUNCTION_NAME_THISCLASS(OnHandleEvent));
	}
	
	UEventModuleStatics::SubscribeEvent<UEventHandle_GameInited>(this, GET_FUNCTION_NAME_THISCLASS(OnGameInited));
	UEventModuleStatics::SubscribeEvent<UEventHandle_GameStarted>(this, GET_FUNCTION_NAME_THISCLASS(OnGameStarted));
	UEventModuleStatics::SubscribeEvent<UEventHandle_GameExited>(this, GET_FUNCTION_NAME_THISCLASS(OnGameExited));
}

void UEventManagerBase::OnPreparatory()
{
	K2_OnPreparatory();
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

bool UEventManagerBase::GetEventInfoByTag(const FGameplayTag& InTag, FEventInfo& OutMenuInfo) const
{
	for(auto& Iter : EventInfos)
	{
		if(Iter.Tag == InTag)
		{
			OutMenuInfo = Iter;
			return true;
		}
	}
	return false;
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
