// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Gameplay/WHGameMode.h"

#include "Main/MainModule.h"

AWHGameMode::AWHGameMode()
{
	
}

void AWHGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if(AMainModule* MainModule = AMainModule::Get())
	{
		MainModule->InitializeModules();
	}
}

void AWHGameMode::BeginPlay()
{
	Super::BeginPlay();
}
