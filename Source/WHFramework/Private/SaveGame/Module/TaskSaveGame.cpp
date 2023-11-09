// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SaveGame/Module/TaskSaveGame.h"

#include "Common/CommonStatics.h"
#include "Task/TaskModule.h"

UTaskSaveGame::UTaskSaveGame()
{
	SaveName = FName("Task");
}

void UTaskSaveGame::OnCreate_Implementation(int32 InIndex)
{
	Super::OnCreate_Implementation(InIndex);

	Refresh();
	SetDefaultData(GetSaveData());
}

void UTaskSaveGame::OnLoad_Implementation(EPhase InPhase)
{
	Super::OnLoad_Implementation(InPhase);

	UTaskModule::Get()->LoadSaveData(GetSaveData(), InPhase);
}

void UTaskSaveGame::OnUnload_Implementation(EPhase InPhase)
{
	Super::OnUnload_Implementation(InPhase);
}

void UTaskSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
	
	SetSaveData(UTaskModule::Get()->GetSaveData(true));
}
