// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/WHGameManager.h"

#include "Common/CommonStatics.h"
#include "Gameplay/WHGameMode.h"

// Sets default values
AWHGameManager::AWHGameManager()
{
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif

	PrimaryActorTick.bCanEverTick = true;
}

void AWHGameManager::OnInitialize_Implementation()
{
	AWHActor::OnInitialize_Implementation();

	if(AWHGameMode* GameMode = UCommonStatics::GetGameMode<AWHGameMode>())
	{
		GameMode->AddManagerToList(this);
	}
}

void AWHGameManager::OnPreparatory_Implementation()
{
	AWHActor::OnPreparatory_Implementation();
}

void AWHGameManager::OnRefresh_Implementation(float DeltaSeconds)
{
	AWHActor::OnRefresh_Implementation(DeltaSeconds);
}

void AWHGameManager::OnTermination_Implementation()
{
	AWHActor::OnTermination_Implementation();

	if(AWHGameMode* GameMode = UCommonStatics::GetGameMode<AWHGameMode>())
	{
		GameMode->RemoveManagerFromList(this);
	}
}
