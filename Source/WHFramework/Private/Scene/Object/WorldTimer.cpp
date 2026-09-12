// Fill out your copyright notice in the Description page of Project Settings.

#include "Scene/Object/WorldTimer.h"

#include "Engine/World.h"
#include "Scene/SceneModuleTypes.h"

UWorldTimer::UWorldTimer()
{
	bAutoSave = true;
}

void UWorldTimer::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);
}

void UWorldTimer::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);
}

void UWorldTimer::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	const auto& SaveData = InSaveData->CastRef<FWorldTimerSaveData>();

	SetDayLength(SaveData.DayLength);
	SetNightLength(SaveData.NightLength);
	if(SaveData.IsSaved())
	{
		SetDateTime(SaveData.DateTime);
	}
	else
	{
		SetCurrentTime(SaveData.TimeOfDay);
	}
}

FSaveData* UWorldTimer::ToData()
{
	FWorldTimerSaveData& SaveData = GetMutableSaveData<FWorldTimerSaveData>();
	SaveData = FWorldTimerSaveData();

	SaveData.DayLength = GetDayLength();
	SaveData.NightLength = GetNightLength();
	SaveData.DateTime = GetDateTime();

	return &SaveData;
}

void UWorldTimer::ResetTimerParams_Implementation() const
{
	ResetDateTime();
}

void UWorldTimer::ResetDateTime_Implementation(float InNewTime) const
{
	const float NewTime = InNewTime == -1.f ? GetCurrentTime() : InNewTime;
	SetDateTime(FDateTime());
	SetCurrentTime(NewTime);
}
