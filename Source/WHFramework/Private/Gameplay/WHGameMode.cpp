// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Gameplay/WHGameMode.h"

#include "Gameplay/WHGameState.h"
#include "Main/MainModule.h"

AWHGameMode::AWHGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AWHGameMode::OnInitialize_Implementation()
{
	if(AMainModule* MainModule = AMainModule::Get())
	{
		MainModule->Execute_OnInitialize(MainModule);
	}
}

void AWHGameMode::OnPreparatory_Implementation(EPhase InPhase)
{
	if(AMainModule* MainModule = AMainModule::Get())
	{
		if(InPhase != EPhase::Final)
		{
			MainModule->Execute_OnPreparatory(MainModule, InPhase);
		}
		else if(MainModule->Execute_IsAutoRunModule(MainModule))
		{
			MainModule->Execute_Run(MainModule);
		}
	}
}

void AWHGameMode::OnRefresh_Implementation(float DeltaSeconds)
{
	if(AMainModule* MainModule = AMainModule::Get())
	{
		if(MainModule->Execute_GetModuleState(MainModule) == EModuleState::Running)
		{
			MainModule->Execute_OnRefresh(MainModule, DeltaSeconds);
		}
	}
}

void AWHGameMode::OnTermination_Implementation()
{
	if(AMainModule* MainModule = AMainModule::Get())
	{
		MainModule->Execute_OnTermination(MainModule);
	}
}

void AWHGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	Execute_OnInitialize(this);
}

void AWHGameMode::InitGameState()
{
	Super::InitGameState();

	if(AWHGameState* WHGameState = Cast<AWHGameState>(GameState))
	{
		WHGameState->Execute_OnInitialize(WHGameState);
	}
}

APlayerController* AWHGameMode::SpawnPlayerController(ENetRole InRemoteRole, const FString& Options)
{
	APlayerController* PlayerController = Super::SpawnPlayerController(InRemoteRole, Options);
	
	if(AWHPlayerController* WHPlayerController = Cast<AWHPlayerController>(PlayerController))
	{
		WHPlayerController->Execute_OnInitialize(WHPlayerController);
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

	Execute_OnPreparatory(this, EPhase::Primary);
	Execute_OnPreparatory(this, EPhase::Lesser);
	Execute_OnPreparatory(this, EPhase::Final);
}

void AWHGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	Execute_OnTermination(this);
}

void AWHGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Execute_OnRefresh(this, DeltaSeconds);
}
