// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SaveGame/Module/InputSaveGame.h"

#include "Common/CommonStatics.h"
#include "Input/InputModule.h"
#include "Main/MainModule.h"

UInputSaveGame::UInputSaveGame()
{
	SaveName = FName("Input");

	ModuleClass = UInputModule::StaticClass();
}

void UInputSaveGame::OnCreate_Implementation(int32 InIndex)
{
	Super::OnCreate_Implementation(InIndex);
}

void UInputSaveGame::OnLoad_Implementation(EPhase InPhase)
{
	Super::OnLoad_Implementation(InPhase);
}

void UInputSaveGame::OnUnload_Implementation(EPhase InPhase)
{
	Super::OnUnload_Implementation(InPhase);
}

void UInputSaveGame::OnRefresh_Implementation()
{
	SetSaveData(AMainModule::GetModuleByClass(false, ModuleClass)->GetSaveData(false));
}
