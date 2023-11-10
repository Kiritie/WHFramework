// Copyright 2021 Sam Carey. All Rights Reserved.

#include "SaveGame/Module/VideoSaveGame.h"

#include "Video/VideoModule.h"

UVideoSaveGame::UVideoSaveGame()
{
	SaveName = FName("Video");

	ModuleClass = UVideoModule::StaticClass();
}

void UVideoSaveGame::OnCreate_Implementation(int32 InIndex)
{
	Super::OnCreate_Implementation(InIndex);
}

void UVideoSaveGame::OnLoad_Implementation(EPhase InPhase)
{
	Super::OnLoad_Implementation(InPhase);
}

void UVideoSaveGame::OnUnload_Implementation(EPhase InPhase)
{
	Super::OnUnload_Implementation(InPhase);
}

void UVideoSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
}
