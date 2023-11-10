// Fill out your copyright notice in the Description page of Project Settings.

#include "Scene/Object/WorldWeather.h"

UWorldWeather::UWorldWeather()
{
}

void UWorldWeather::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void UWorldWeather::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
}

void UWorldWeather::UpdateWeather(float DeltaSeconds)
{
	
}
