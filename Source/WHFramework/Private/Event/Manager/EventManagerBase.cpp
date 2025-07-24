// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Manager/EventManagerBase.h"

#include "Event/EventModuleStatics.h"
#include "Event/Handle/EventHandleBase.h"

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
