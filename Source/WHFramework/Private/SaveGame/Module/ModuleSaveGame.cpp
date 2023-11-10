// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SaveGame/Module/ModuleSaveGame.h"

#include "Common/CommonStatics.h"
#include "Main/MainModule.h"

UModuleSaveGame::UModuleSaveGame()
{
	SaveName = FName("Module");

	ModuleClass = nullptr;
}

void UModuleSaveGame::OnCreate_Implementation(int32 InIndex)
{
	Super::OnCreate_Implementation(InIndex);

	Refresh();
	SetDefaultData(GetSaveData());
}

void UModuleSaveGame::OnLoad_Implementation(EPhase InPhase)
{
	Super::OnLoad_Implementation(InPhase);

	AMainModule::GetModuleByClass(false, ModuleClass)->LoadSaveData(GetSaveData(), InPhase);
}

void UModuleSaveGame::OnUnload_Implementation(EPhase InPhase)
{
	Super::OnUnload_Implementation(InPhase);
}

void UModuleSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
	
	SetSaveData(AMainModule::GetModuleByClass(false, ModuleClass)->GetSaveData(true));
}
