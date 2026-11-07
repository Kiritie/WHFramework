#include "Achievement/AchievementModuleStatics.h"

#include "Achievement/AchievementModule.h"
#include "Engine/Engine.h"

namespace
{
	UAchievementModule* FindAchievements(const UObject* Context)
	{
		UWorld* World = GEngine && Context ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
		return World ? UAchievementModule::GetPtr(World) : nullptr;
	}
}

bool UAchievementModuleStatics::IsAchievementExists(const UObject* WorldContextObject, FName InKey)
{
	const UAchievementModule* Module = FindAchievements(WorldContextObject);
	return Module && Module->IsAchievementExists(InKey);
}

int32 UAchievementModuleStatics::GetAchievedAchievementCount(const UObject* WorldContextObject)
{
	const UAchievementModule* Module = FindAchievements(WorldContextObject);
	return Module ? Module->GetAchievedAchievementCount() : 0;
}

float UAchievementModuleStatics::GetAchievedAchievementPercentage(const UObject* WorldContextObject)
{
	const UAchievementModule* Module = FindAchievements(WorldContextObject);
	return Module ? Module->GetAchievedAchievementPercentage() : 0.f;
}

void UAchievementModuleStatics::GetAchievementState(const UObject* WorldContextObject, FName InKey, bool& OutFoundAchievement, FAchievementData& OutData, FAchievementStates& OutState)
{
	OutFoundAchievement = false;
	OutData = FAchievementData();
	OutState = FAchievementStates();
	if (const UAchievementModule* Module = FindAchievements(WorldContextObject))
	{
		Module->GetAchievementState(InKey, OutFoundAchievement, OutData, OutState);
	}
}

void UAchievementModuleStatics::UnlockAchievement(const UObject* WorldContextObject, FName InKey)
{
	if (UAchievementModule* Module = FindAchievements(WorldContextObject)) Module->UnlockAchievement(InKey);
}

void UAchievementModuleStatics::SetAchievementProgress(const UObject* WorldContextObject, FName InKey, float InProgress)
{
	if (UAchievementModule* Module = FindAchievements(WorldContextObject)) Module->SetAchievementProgress(InKey, InProgress);
}

void UAchievementModuleStatics::AddAchievementProgress(const UObject* WorldContextObject, FName InKey, float InProgress)
{
	if (UAchievementModule* Module = FindAchievements(WorldContextObject)) Module->AddAchievementProgress(InKey, InProgress);
}
