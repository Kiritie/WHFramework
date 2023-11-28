// Fill out your copyright notice in the Description page of Project Settings.

#include "Scene/Object/WorldWeather.h"

UWorldWeather::UWorldWeather()
{
}

void UWorldWeather::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void UWorldWeather::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
}

void UWorldWeather::UpdateWeather(float DeltaSeconds)
{
	
}
