// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SaveGame/Module/GeneralSaveGame.h"

#include "Common/CommonStatics.h"
#include "SaveGame/SaveGameModule.h"

UGeneralSaveGame::UGeneralSaveGame()
{
	SaveName = FName("General");

	ModuleClass = USaveGameModule::StaticClass();
}

void UGeneralSaveGame::OnCreate_Implementation(int32 InIndex)
{
	Super::OnCreate_Implementation(InIndex);
}

void UGeneralSaveGame::OnLoad_Implementation(EPhase InPhase)
{
	Super::OnLoad_Implementation(InPhase);
}

void UGeneralSaveGame::OnUnload_Implementation(EPhase InPhase)
{
	Super::OnUnload_Implementation(InPhase);
}

void UGeneralSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
}
