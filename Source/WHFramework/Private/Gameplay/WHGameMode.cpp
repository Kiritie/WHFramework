// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Gameplay/WHGameMode.h"

#include "Gameplay/WHGameState.h"
#include "Main/MainModule.h"

AWHGameMode::AWHGameMode()
{
	
}

void AWHGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	AMainModule* MainModule = AMainModule::Get();
	if(MainModule && MainModule->IsValidLowLevel())
	{
		MainModule->InitializeModules();
	}
}

void AWHGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void AWHGameMode::InitGameState()
{
	Super::InitGameState();

	if(AWHGameState* WHGameState = Cast<AWHGameState>(GameState))
	{
		WHGameState->Initialize();
	}
}

APlayerController* AWHGameMode::SpawnPlayerController(ENetRole InRemoteRole, const FString& Options)
{
	APlayerController* PlayerController = Super::SpawnPlayerController(InRemoteRole, Options);
	
	if(AWHPlayerController* WHPlayerController = Cast<AWHPlayerController>(PlayerController))
	{
		WHPlayerController->Initialize();
	}
	return PlayerController;
}
