// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SaveGame/Module/SettingSaveGame.h"

#include "Setting/SettingModule.h"

USettingSaveGame::USettingSaveGame()
{
	SaveName = FName("Setting");

	ModuleClass = USettingModule::StaticClass();
}

void USettingSaveGame::OnCreate_Implementation(int32 InIndex)
{
	Super::OnCreate_Implementation(InIndex);
}

void USettingSaveGame::OnLoad_Implementation(EPhase InPhase)
{
	Super::OnLoad_Implementation(InPhase);
}

void USettingSaveGame::OnUnload_Implementation(EPhase InPhase)
{
	Super::OnUnload_Implementation(InPhase);
}

void USettingSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
}
