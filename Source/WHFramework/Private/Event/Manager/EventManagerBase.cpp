// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Manager/EventManagerBase.h"

#include "Event/EventModuleStatics.h"
#include "Event/Handle/Common/EventHandle_StartGame.h"
#include "Event/Handle/Common/EventHandle_ExitGame.h"
#include "Event/Handle/Common/EventHandle_InitGame.h"

// ParamSets default values
UEventManagerBase::UEventManagerBase()
{
}

void UEventManagerBase::OnInitialize_Implementation()
{
	UEventModuleStatics::SubscribeEvent<UEventHandle_InitGame>(this, FName("OnInitGame"));
	UEventModuleStatics::SubscribeEvent<UEventHandle_StartGame>(this, FName("OnStartGame"));
	UEventModuleStatics::SubscribeEvent<UEventHandle_ExitGame>(this, FName("OnExitGame"));
}

void UEventManagerBase::OnInitGame_Implementation(UObject* InSender, UEventHandle_InitGame* InEventHandle)
{
	
}

void UEventManagerBase::OnStartGame_Implementation(UObject* InSender, UEventHandle_StartGame* InEventHandle)
{
	
}

void UEventManagerBase::OnExitGame_Implementation(UObject* InSender, UEventHandle_ExitGame* InEventHandle)
{
	
}
