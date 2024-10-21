// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/WHWorldSubsystem.h"

#include "EngineUtils.h"
#include "Main/MainModule.h"

UWHWorldSubsystem::UWHWorldSubsystem()
{
}

void UWHWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	for(TActorIterator<AActor> Iter(&InWorld); Iter; ++Iter)
	{
		AActor* Actor = *Iter;
		if(Actor->Implements<UWHActorInterface>())
		{
			IWHActorInterface::Execute_OnInitialize(Actor);
		}
	}
}
