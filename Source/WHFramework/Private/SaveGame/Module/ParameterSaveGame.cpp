// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SaveGame/Module/ParameterSaveGame.h"

#include "Common/CommonBPLibrary.h"
#include "Parameter/ParameterModule.h"

UParameterSaveGame::UParameterSaveGame()
{
	SaveName = FName("Parameter");
}

void UParameterSaveGame::OnCreate_Implementation(int32 InIndex)
{
	Super::OnCreate_Implementation(InIndex);

	Refresh();
	SetDefaultData(GetSaveData());
}

void UParameterSaveGame::OnLoad_Implementation(EPhase InPhase)
{
	Super::OnLoad_Implementation(InPhase);

	AParameterModule::Get()->LoadSaveData(GetSaveData(), InPhase);
}

void UParameterSaveGame::OnUnload_Implementation(EPhase InPhase)
{
	Super::OnUnload_Implementation(InPhase);
}

void UParameterSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
	
	SetSaveData(AParameterModule::Get()->GetSaveData(true));
}
