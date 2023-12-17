// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SaveGame/Module/SceneSaveGame.h"

#include "Common/CommonStatics.h"
#include "Scene/SceneModule.h"
#include "Main/MainModule.h"

USceneSaveGame::USceneSaveGame()
{
	SaveName = FName("Scene");

	ModuleClass = USceneModule::StaticClass();
}

void USceneSaveGame::OnCreate_Implementation(int32 InIndex)
{
	Super::OnCreate_Implementation(InIndex);
}

void USceneSaveGame::OnLoad_Implementation(EPhase InPhase)
{
	Super::OnLoad_Implementation(InPhase);
}

void USceneSaveGame::OnUnload_Implementation(EPhase InPhase)
{
	Super::OnUnload_Implementation(InPhase);
}

void USceneSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
}
