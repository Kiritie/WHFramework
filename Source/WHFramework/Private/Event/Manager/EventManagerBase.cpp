// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Manager/EventManagerBase.h"

#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Common/EventHandle_StartGame.h"
#include "Event/Handle/Common/EventHandle_ExitGame.h"
#include "Event/Handle/Common/EventHandle_InitGame.h"

// ParamSets default values
UEventManagerBase::UEventManagerBase()
{
}

void UEventManagerBase::OnInitialize_Implementation()
{
	UEventModuleBPLibrary::SubscribeEvent<UEventHandle_InitGame>(this, FName("OnInitPlay"));
	UEventModuleBPLibrary::SubscribeEvent<UEventHandle_StartGame>(this, FName("OnBeginPlay"));
	UEventModuleBPLibrary::SubscribeEvent<UEventHandle_ExitGame>(this, FName("OnEndPlay"));
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
