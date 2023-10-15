// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Manager/EventManagerBase.h"

#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Common/EventHandle_BeginPlay.h"
#include "Event/Handle/Common/EventHandle_EndPlay.h"

// ParamSets default values
UEventManagerBase::UEventManagerBase()
{
}

void UEventManagerBase::OnInitialize_Implementation()
{
	UEventModuleBPLibrary::SubscribeEvent<UEventHandle_BeginPlay>(this, FName("OnBeginPlay"));
	UEventModuleBPLibrary::SubscribeEvent<UEventHandle_EndPlay>(this, FName("OnEndPlay"));
}

void UEventManagerBase::OnBeginPlay_Implementation(UObject* InSender, UEventHandle_BeginPlay* InEventHandle)
{
	
}

void UEventManagerBase::OnEndPlay_Implementation(UObject* InSender, UEventHandle_EndPlay* InEventHandle)
{
	
}
