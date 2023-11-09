// Fill out your copyright notice in the Description page of Project Settings.


#include "Achievement/AchievementModuleStatics.h"

#include "Achievement/AchievementModule.h"

bool UAchievementModuleStatics::IsAchievementExists(FName InKey)
{
	if(UAchievementModule* AchievementModule = UAchievementModule::Get())
	{
		return AchievementModule->IsAchievementExists(InKey);
	}
	return false;
}

int32 UAchievementModuleStatics::GetAchievedAchievementCount()
{
	if(UAchievementModule* AchievementModule = UAchievementModule::Get())
	{
		return AchievementModule->GetAchievedAchievementCount();
	}
	return 0;
}

float UAchievementModuleStatics::GetAchievedAchievementPercentage()
{
	if(UAchievementModule* AchievementModule = UAchievementModule::Get())
	{
		return AchievementModule->GetAchievedAchievementPercentage();
	}
	return 0.f;
}

void UAchievementModuleStatics::GetAchievementState(FName InKey, bool& OutFoundAchievement, FAchievementData& OutData, FAchievementStates& OutState)
{
	if(UAchievementModule* AchievementModule = UAchievementModule::Get())
	{
		AchievementModule->GetAchievementState(InKey, OutFoundAchievement, OutData, OutState);
	}
}

void UAchievementModuleStatics::UnlockAchievement(FName InKey)
{
	if(UAchievementModule* AchievementModule = UAchievementModule::Get())
	{
		AchievementModule->UnlockAchievement(InKey);
	}
}

void UAchievementModuleStatics::SetAchievementProgress(FName InKey, float InProgress)
{
	if(UAchievementModule* AchievementModule = UAchievementModule::Get())
	{
		AchievementModule->SetAchievementProgress(InKey, InProgress);
	}
}

void UAchievementModuleStatics::AddAchievementProgress(FName InKey, float InProgress)
{
	if(UAchievementModule* AchievementModule = UAchievementModule::Get())
	{
		AchievementModule->AddAchievementProgress(InKey, InProgress);
	}
}
