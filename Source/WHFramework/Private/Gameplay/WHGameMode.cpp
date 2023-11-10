// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Gameplay/WHGameMode.h"

#include "Gameplay/WHGameState.h"
#include "Gameplay/WHPlayerController.h"
#include "Main/MainModule.h"

AWHGameMode::AWHGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	DefaultPawnClass = nullptr;
	PlayerControllerClass = AWHPlayerController::StaticClass();
	GameStateClass = AWHGameState::StaticClass();
}

void AWHGameMode::OnInitialize_Implementation()
{
	if(AMainModule* MainModule = AMainModule::GetPtr())
	{
		MainModule->Execute_OnInitialize(MainModule);
	}
}

void AWHGameMode::OnPreparatory_Implementation(EPhase InPhase)
{
	if(AMainModule* MainModule = AMainModule::GetPtr())
	{
		MainModule->Execute_OnPreparatory(MainModule, InPhase);
	}
}

void AWHGameMode::OnRefresh_Implementation(float DeltaSeconds)
{
	if(AMainModule* MainModule = AMainModule::GetPtr())
	{
		MainModule->Execute_OnRefresh(MainModule, DeltaSeconds);
	}
}

void AWHGameMode::OnTermination_Implementation(EPhase InPhase)
{
	if(AMainModule* MainModule = AMainModule::GetPtr())
	{
		MainModule->Execute_OnTermination(MainModule, InPhase);
	}
}

void AWHGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnInitialize(this);
	}
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

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnPreparatory(this, EPhase::Primary);
		Execute_OnPreparatory(this, EPhase::Lesser);
		Execute_OnPreparatory(this, EPhase::Final);
	}
}

void AWHGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnTermination(this, EPhase::Primary);
		Execute_OnTermination(this, EPhase::Lesser);
		Execute_OnTermination(this, EPhase::Final);
	}
}

void AWHGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}
