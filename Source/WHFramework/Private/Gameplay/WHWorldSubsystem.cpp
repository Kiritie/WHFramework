// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/WHWorldSubsystem.h"

#include "Main/MainModule.h"

UWHWorldSubsystem::UWHWorldSubsystem()
{
}

void UWHWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if(AMainModule* MainModule = AMainModule::GetPtr())
	{
		MainModule->Execute_OnInitialize(MainModule);
	}
}
