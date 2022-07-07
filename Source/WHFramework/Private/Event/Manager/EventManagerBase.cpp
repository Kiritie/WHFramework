// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Manager/EventManagerBase.h"

#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Global/EventHandle_BeginPlay.h"
#include "Event/Handle/Global/EventHandle_EndPlay.h"
#include "Global/GlobalTypes.h"

// ParamSets default values
AEventManagerBase::AEventManagerBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
}

void AEventManagerBase::OnInitialize_Implementation()
{
	UEventModuleBPLibrary::SubscribeEvent(UEventHandle_BeginPlay::StaticClass(), this, FName("OnBeginPlay"));
	UEventModuleBPLibrary::SubscribeEvent(UEventHandle_EndPlay::StaticClass(), this, FName("OnEndPlay"));
}

void AEventManagerBase::OnBeginPlay_Implementation(UObject* InSender, UEventHandle_BeginPlay* InEventHandle)
{
	GIsPlaying = true;
	GIsSimulating = InEventHandle->bIsSimulating;
}

void AEventManagerBase::OnEndPlay_Implementation(UObject* InSender, UEventHandle_EndPlay* InEventHandle)
{
	GIsPlaying = false;
	GIsSimulating = false;
}
