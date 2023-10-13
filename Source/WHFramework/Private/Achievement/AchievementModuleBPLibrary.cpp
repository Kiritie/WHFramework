// Fill out your copyright notice in the Description page of Project Settings.


#include "Achievement/AchievementModuleBPLibrary.h"

#include "Achievement/AchievementModule.h"

bool UAchievementModuleBPLibrary::IsAchievementExists(FName InKey)
{
	if(AAchievementModule* AchievementModule = AAchievementModule::Get())
	{
		return AchievementModule->IsAchievementExists(InKey);
	}
	return false;
}

int32 UAchievementModuleBPLibrary::GetAchievedAchievementCount()
{
	if(AAchievementModule* AchievementModule = AAchievementModule::Get())
	{
		return AchievementModule->GetAchievedAchievementCount();
	}
	return 0;
}

float UAchievementModuleBPLibrary::GetAchievedAchievementPercentage()
{
	if(AAchievementModule* AchievementModule = AAchievementModule::Get())
	{
		return AchievementModule->GetAchievedAchievementPercentage();
	}
	return 0.f;
}

void UAchievementModuleBPLibrary::GetAchievementState(FName InKey, bool& OutFoundAchievement, FAchievementData& OutData, FAchievementStates& OutState)
{
	if(AAchievementModule* AchievementModule = AAchievementModule::Get())
	{
		AchievementModule->GetAchievementState(InKey, OutFoundAchievement, OutData, OutState);
	}
}

void UAchievementModuleBPLibrary::UnlockAchievement(FName InKey)
{
	if(AAchievementModule* AchievementModule = AAchievementModule::Get())
	{
		AchievementModule->UnlockAchievement(InKey);
	}
}

void UAchievementModuleBPLibrary::SetAchievementProgress(FName InKey, float InProgress)
{
	if(AAchievementModule* AchievementModule = AAchievementModule::Get())
	{
		AchievementModule->SetAchievementProgress(InKey, InProgress);
	}
}

void UAchievementModuleBPLibrary::AddAchievementProgress(FName InKey, float InProgress)
{
	if(AAchievementModule* AchievementModule = AAchievementModule::Get())
	{
		AchievementModule->AddAchievementProgress(InKey, InProgress);
	}
}
