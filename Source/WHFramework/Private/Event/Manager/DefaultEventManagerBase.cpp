// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Manager/DefaultEventManagerBase.h"

#include "Event/EventModuleStatics.h"
#include "Event/Handle/Common/Game/EventHandle_GameStarted.h"
#include "Event/Handle/Common/Game/EventHandle_GameExited.h"
#include "Event/Handle/Common/Game/EventHandle_GameInited.h"

// ParamSets default values
UDefaultEventManagerBase::UDefaultEventManagerBase()
{
	EventManagerName = FName("Default");
}

void UDefaultEventManagerBase::OnInitialize()
{
	Super::OnInitialize();
	
	UEventModuleStatics::SubscribeEvent<UEventHandle_GameInited>(this, GET_FUNCTION_NAME_THISCLASS(OnGameInited));
	UEventModuleStatics::SubscribeEvent<UEventHandle_GameStarted>(this, GET_FUNCTION_NAME_THISCLASS(OnGameStarted));
	UEventModuleStatics::SubscribeEvent<UEventHandle_GameExited>(this, GET_FUNCTION_NAME_THISCLASS(OnGameExited));
}

void UDefaultEventManagerBase::OnPreparatory()
{
	Super::OnPreparatory();
}

void UDefaultEventManagerBase::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UDefaultEventManagerBase::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
}

void UDefaultEventManagerBase::OnHandleEvent(UObject* InSender, UEventHandleBase* InEventHandle)
{
	Super::OnHandleEvent(InSender, InEventHandle);
}

void UDefaultEventManagerBase::OnGameInited_Implementation(UObject* InSender, UEventHandle_GameInited* InEventHandle)
{
}

void UDefaultEventManagerBase::OnGameExited_Implementation(UObject* InSender, UEventHandle_GameExited* InEventHandle)
{
}

void UDefaultEventManagerBase::OnGameStarted_Implementation(UObject* InSender, UEventHandle_GameStarted* InEventHandle)
{
}
