// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SaveGame/Module/InputSaveGame.h"

#include "Common/CommonStatics.h"
#include "Input/InputModule.h"

UInputSaveGame::UInputSaveGame()
{
	SaveName = FName("Input");
}

void UInputSaveGame::OnCreate_Implementation(int32 InIndex)
{
	Super::OnCreate_Implementation(InIndex);

	Refresh();
	SetDefaultData(GetSaveData());
}

void UInputSaveGame::OnLoad_Implementation(EPhase InPhase)
{
	Super::OnLoad_Implementation(InPhase);

	UInputModule::Get().LoadSaveData(GetSaveData(), InPhase);
}

void UInputSaveGame::OnUnload_Implementation(EPhase InPhase)
{
	Super::OnUnload_Implementation(InPhase);
}

void UInputSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
	
	SetSaveData(UInputModule::Get().GetSaveData(true));
}
