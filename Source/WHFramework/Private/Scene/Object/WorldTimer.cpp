// Fill out your copyright notice in the Description page of Project Settings.

#include "Scene/Object/WorldTimer.h"

#include "Engine/World.h"
#include "Scene/SceneModuleTypes.h"

UWorldTimer::UWorldTimer()
{
	bAutoSave = true;
}

void UWorldTimer::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void UWorldTimer::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
}

void UWorldTimer::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	const auto& SaveData = InSaveData->CastRef<FWorldTimerSaveData>();

	SetDayLength(SaveData.DayLength);
	SetNightLength(SaveData.NightLength);
	if(SaveData.DateTime != -1)
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
	static FWorldTimerSaveData* SaveData;
	SaveData = new FWorldTimerSaveData();

	SaveData->DayLength = GetDayLength();
	SaveData->NightLength = GetNightLength();
	SaveData->TimeOfDay = GetCurrentTime();
	SaveData->DateTime = GetDateTime();

	return SaveData;
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
