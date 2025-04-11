// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Gameplay/WHGameMode.h"

#include "Debug/Widget/SDebugPanelWidget.h"
#include "Gameplay/WHGameManager.h"
#include "Gameplay/WHGameState.h"
#include "Gameplay/WHHUD.h"
#include "Gameplay/WHPlayerController.h"
#include "Gameplay/WHPlayerState.h"
#include "Main/MainModule.h"
#include "Widget/WidgetModuleStatics.h"

AWHGameMode::AWHGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	DefaultPawnClass = nullptr;
	HUDClass = AWHHUD::StaticClass();
	PlayerControllerClass = AWHPlayerController::StaticClass();
	GameStateClass = AWHGameState::StaticClass();
	PlayerStateClass = AWHPlayerState::StaticClass();
	DefaultManagerClass = nullptr;

	bInitialized = false;
}

void AWHGameMode::OnInitialize_Implementation()
{
	bInitialized = true;
	
	if(DefaultManagerClass)
	{
		GetWorld()->SpawnActor(DefaultManagerClass);
	}
}

void AWHGameMode::OnPreparatory_Implementation()
{
	if(AMainModule* MainModule = AMainModule::GetPtr())
	{
		MainModule->Execute_OnPreparatory(MainModule);
	}
}

void AWHGameMode::OnRefresh_Implementation(float DeltaSeconds)
{
	
}

void AWHGameMode::OnTermination_Implementation()
{
	
}

bool AWHGameMode::ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor)
{
	if(FPlatformString::Stricmp(Cmd, TEXT("WH.Debug")) == 0)
	{
		UWidgetModuleStatics::ToggleSlateWidget<SDebugPanelWidget>(true);
		return true;
	}
	return Super::ProcessConsoleExec(Cmd, Ar, Executor);
}

void AWHGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
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

void AWHGameMode::InitializeHUDForPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::InitializeHUDForPlayer_Implementation(NewPlayer);

	if(AWHHUD* WHHUD = Cast<AWHHUD>(NewPlayer->GetHUD()))
	{
		WHHUD->Execute_OnInitialize(WHHUD);
	}
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

	if(Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnPreparatory(this);
	}
}

void AWHGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if(Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnTermination(this);
	}
}

void AWHGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if(Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}

AWHGameManager* AWHGameMode::GetManagerByClass(TSubclassOf<AWHGameManager> InClass)
{
	if(!InClass) return nullptr;
	
	const FName ManagerName = InClass.GetDefaultObject()->GetManagerName();
	return GetManagerByName(ManagerName);
}

AWHGameManager* AWHGameMode::GetManagerByName(const FName InName) const
{
	if(ManagerMap.Contains(InName))
	{
		return ManagerMap[InName];
	}
	return nullptr;
}

void AWHGameMode::AddManagerToList(AWHGameManager* InManager)
{
	if(!Managers.Contains(InManager))
	{
		Managers.Add(InManager);
		if(!ManagerMap.Contains(InManager->GetManagerName()))
		{
			ManagerMap.Add(InManager->GetManagerName(), InManager);
		}
	}
}

void AWHGameMode::RemoveManagerFromList(AWHGameManager* InManager)
{
	if(Managers.Contains(InManager))
	{
		Managers.Remove(InManager);
		if(ManagerMap.Contains(InManager->GetManagerName()))
		{
			ManagerMap.Remove(InManager->GetManagerName());
		}
	}
}
