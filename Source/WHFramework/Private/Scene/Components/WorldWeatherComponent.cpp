// Fill out your copyright notice in the Description page of Project Settings.

#include "Scene/Components/WorldWeatherComponent.h"

UWorldWeatherComponent::UWorldWeatherComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldWeatherComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UWorldWeatherComponent::UpdateWeather(float DeltaSeconds)
{
	
}
