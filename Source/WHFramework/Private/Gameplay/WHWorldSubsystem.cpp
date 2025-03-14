// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/WHWorldSubsystem.h"

#include "EngineUtils.h"
#include "Gameplay/WHGameManager.h"
#include "Main/MainModule.h"

UWHWorldSubsystem::UWHWorldSubsystem()
{
	
}

void UWHWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if(AMainModule* MainModule = AMainModule::GetPtr())
	{
		if(!IWHActorInterface::Execute_IsInitialized(MainModule))
		{
			IWHActorInterface::Execute_OnInitialize(MainModule);
		}
	}

	for(TActorIterator<AWHGameManager> Iter(&InWorld); Iter; ++Iter)
	{
		AWHGameManager* GameManager = *Iter;
		if(!IWHActorInterface::Execute_IsInitialized(GameManager))
		{
			IWHActorInterface::Execute_OnInitialize(GameManager);
		}
	}

	for(TActorIterator<AActor> Iter(&InWorld); Iter; ++Iter)
	{
		AActor* Actor = *Iter;
		if(Actor->Implements<UWHActorInterface>() && !IWHActorInterface::Execute_IsInitialized(Actor))
		{
			IWHActorInterface::Execute_OnInitialize(Actor);
		}
	}
}
