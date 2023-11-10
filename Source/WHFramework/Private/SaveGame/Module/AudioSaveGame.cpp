// Copyright 2021 Sam Carey. All Rights Reserved.

#include "SaveGame/Module/AudioSaveGame.h"

#include "Audio/AudioModule.h"

UAudioSaveGame::UAudioSaveGame()
{
	SaveName = FName("Audio");
}

void UAudioSaveGame::OnCreate_Implementation(int32 InIndex)
{
	Super::OnCreate_Implementation(InIndex);

	Refresh();
	SetDefaultData(GetSaveData());
}

void UAudioSaveGame::OnLoad_Implementation(EPhase InPhase)
{
	Super::OnLoad_Implementation(InPhase);

	UAudioModule::Get().LoadSaveData(GetSaveData(), InPhase);
}

void UAudioSaveGame::OnUnload_Implementation(EPhase InPhase)
{
	Super::OnUnload_Implementation(InPhase);
}

void UAudioSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
	
	SetSaveData(UAudioModule::Get().GetSaveData(true));
}
