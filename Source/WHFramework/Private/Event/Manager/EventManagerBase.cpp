// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Manager/EventManagerBase.h"

#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Common/EventHandle_BeginPlay.h"
#include "Event/Handle/Common/EventHandle_EndPlay.h"

// ParamSets default values
AEventManagerBase::AEventManagerBase()
{
}

void AEventManagerBase::OnInitialize_Implementation()
{
	UEventModuleBPLibrary::SubscribeEvent<UEventHandle_BeginPlay>(this, FName("OnBeginPlay"));
	UEventModuleBPLibrary::SubscribeEvent<UEventHandle_EndPlay>(this, FName("OnEndPlay"));
}

void AEventManagerBase::OnBeginPlay_Implementation(UObject* InSender, UEventHandle_BeginPlay* InEventHandle)
{
	
}

void AEventManagerBase::OnEndPlay_Implementation(UObject* InSender, UEventHandle_EndPlay* InEventHandle)
{
	
}
