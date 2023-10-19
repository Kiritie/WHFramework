// Copyright 2021 Sam Carey. All Rights Reserved.

#include "SaveGame/Module/CameraSaveGame.h"

#include "Camera/CameraModule.h"

UCameraSaveGame::UCameraSaveGame()
{
	SaveName = FName("Camera");
}

void UCameraSaveGame::OnCreate_Implementation(int32 InIndex)
{
	Super::OnCreate_Implementation(InIndex);

	Refresh();
}

void UCameraSaveGame::OnLoad_Implementation(EPhase InPhase)
{
	Super::OnLoad_Implementation(InPhase);

	ACameraModule::Get()->LoadSaveData(GetSaveData(), InPhase);
}

void UCameraSaveGame::OnUnload_Implementation(EPhase InPhase)
{
	Super::OnUnload_Implementation(InPhase);
}

void UCameraSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
	
	SetSaveData(ACameraModule::Get()->GetSaveData(true));
}
