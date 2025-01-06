// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SaveGame/Module/CharacterSaveGame.h"

#include "Common/CommonStatics.h"
#include "Character/CharacterModule.h"

UCharacterSaveGame::UCharacterSaveGame()
{
	SaveName = FName("Character");

	ModuleClass = UCharacterModule::StaticClass();
}

void UCharacterSaveGame::OnCreate_Implementation(int32 InIndex)
{
	Super::OnCreate_Implementation(InIndex);
}

void UCharacterSaveGame::OnLoad_Implementation(EPhase InPhase)
{
	Super::OnLoad_Implementation(InPhase);
}

void UCharacterSaveGame::OnUnload_Implementation(EPhase InPhase)
{
	Super::OnUnload_Implementation(InPhase);
}

void UCharacterSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
}
