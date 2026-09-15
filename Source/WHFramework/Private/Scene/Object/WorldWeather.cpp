// Fill out your copyright notice in the Description page of Project Settings.

#include "Scene/Object/WorldWeather.h"

#include "Scene/SceneModuleTypes.h"

UWorldWeather::UWorldWeather()
{
	bAutoSave = true;
}

void UWorldWeather::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);
}

void UWorldWeather::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);
}

void UWorldWeather::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	const auto& SaveData = InSaveData.GetRef<FWorldWeatherSaveData>();

	SetWeatherSeed(SaveData.WeatherSeed);
	if(!SaveData.WeatherParams.IsEmpty())
	{
		SetWeatherParams(SaveData.WeatherParams);
	}
	else
	{
		ResetWeatherParams();
	}
}

FParameter UWorldWeather::ToData()
{
	FWorldWeatherSaveData SaveData;

	SaveData.WeatherSeed = GetWeatherSeed();
	SaveData.WeatherParams = GetWeatherParams();

	return FParameter(MoveTemp(SaveData));
}
