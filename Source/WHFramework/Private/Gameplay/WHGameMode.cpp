// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Gameplay/WHGameMode.h"

#include "Gameplay/WHGameState.h"
#include "Main/MainModule.h"

AWHGameMode::AWHGameMode()
{
	
}

void AWHGameMode::OnInitialize_Implementation()
{
	if(AMainModule* MainModule = AMainModule::Get())
	{
		MainModule->InitializeModules();
	}
}

void AWHGameMode::OnPreparatory_Implementation()
{
	
}

void AWHGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	OnInitialize();
}

void AWHGameMode::InitGameState()
{
	Super::InitGameState();

	if(AWHGameState* WHGameState = Cast<AWHGameState>(GameState))
	{
		WHGameState->OnInitialize();
	}
}

APlayerController* AWHGameMode::SpawnPlayerController(ENetRole InRemoteRole, const FString& Options)
{
	APlayerController* PlayerController = Super::SpawnPlayerController(InRemoteRole, Options);
	
	if(AWHPlayerController* WHPlayerController = Cast<AWHPlayerController>(PlayerController))
	{
		WHPlayerController->OnInitialize();
	}
	return PlayerController;
}

APlayerController* AWHGameMode::Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	return Super::Login(NewPlayer, InRemoteRole, Portal, Options, UniqueId, ErrorMessage);
}

void AWHGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
}

void AWHGameMode::BeginPlay()
{
	Super::BeginPlay();

	OnPreparatory();
}
