// Fill out your copyright notice in the Description page of Project Settings.


#include "Achievement/AchievementModuleStatics.h"

#include "Achievement/AchievementModule.h"

bool UAchievementModuleStatics::IsAchievementExists(FName InKey)
{
	return UAchievementModule::Get().IsAchievementExists(InKey);
}

int32 UAchievementModuleStatics::GetAchievedAchievementCount()
{
	return UAchievementModule::Get().GetAchievedAchievementCount();
}

float UAchievementModuleStatics::GetAchievedAchievementPercentage()
{
	return UAchievementModule::Get().GetAchievedAchievementPercentage();
}

void UAchievementModuleStatics::GetAchievementState(FName InKey, bool& OutFoundAchievement, FAchievementData& OutData, FAchievementStates& OutState)
{
	UAchievementModule::Get().GetAchievementState(InKey, OutFoundAchievement, OutData, OutState);
}

void UAchievementModuleStatics::UnlockAchievement(FName InKey)
{
	UAchievementModule::Get().UnlockAchievement(InKey);
}

void UAchievementModuleStatics::SetAchievementProgress(FName InKey, float InProgress)
{
	UAchievementModule::Get().SetAchievementProgress(InKey, InProgress);
}

void UAchievementModuleStatics::AddAchievementProgress(FName InKey, float InProgress)
{
	UAchievementModule::Get().AddAchievementProgress(InKey, InProgress);
}
