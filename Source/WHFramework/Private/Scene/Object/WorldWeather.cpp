// Fill out your copyright notice in the Description page of Project Settings.

#include "Scene/Object/WorldWeather.h"

#include "Scene/SceneModuleTypes.h"

UWorldWeather::UWorldWeather()
{
	bAutoSave = true;
}

void UWorldWeather::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void UWorldWeather::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
}

void UWorldWeather::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	const auto& SaveData = InSaveData->CastRef<FWorldWeatherSaveData>();

	SetWeatherSeed(SaveData.WeatherSeed);
	if(!SaveData.WeatherParams.IsEmpty()) SetWeatherParams(SaveData.WeatherParams);
}

FSaveData* UWorldWeather::ToData()
{
	static FWorldWeatherSaveData* SaveData;
	SaveData = new FWorldWeatherSaveData();

	SaveData->WeatherSeed = GetWeatherSeed();
	SaveData->WeatherParams = GetWeatherParams();

	return SaveData;
}
