// Copyright 2021 Sam Carey. All Rights Reserved.

#include "SaveGame/Module/AchievementSaveGame.h"

#include "Achievement/AchievementModule.h"

UAchievementSaveGame::UAchievementSaveGame()
{
	SaveName = FName("Achievement");
}

void UAchievementSaveGame::OnCreate_Implementation(int32 InIndex)
{
	Super::OnCreate_Implementation(InIndex);

	SaveData.States.SetNum(AAchievementModule::Get()->Achievements.Num());
}

void UAchievementSaveGame::OnLoad_Implementation(EPhase InPhase)
{
	Super::OnLoad_Implementation(InPhase);

	AAchievementModule::Get()->LoadSaveData(GetSaveData(), InPhase);
}

void UAchievementSaveGame::OnUnload_Implementation(EPhase InPhase)
{
	Super::OnUnload_Implementation(InPhase);
}

void UAchievementSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
	
	SetSaveData(AAchievementModule::Get()->GetSaveData(true));
}
