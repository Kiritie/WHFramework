// Fill out your copyright notice in the Description page of Project Settings.

#include "Scene/Components/WorldTimerComponent.h"
#include "Engine/World.h"

UWorldTimerComponent::UWorldTimerComponent()
{
	SkyLight = nullptr;
	SunLight = nullptr;

	SecondsOfDay = 300;
	TimeSeconds = 0;
	SunriseTime = 6;
	SunsetTime = 18;
	CurrentDay = 0;
	CurrentHour = 0;
	CurrentMinute = 0;
	CurrentSeconds = 0;
}

void UWorldTimerComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UWorldTimerComponent::SetTimeSeconds(int InTimeSeconds, bool bUpdateTimer /*= true*/)
{
	TimeSeconds = InTimeSeconds;
	if (bUpdateTimer)
	{
		UpdateTimer();
	}
}
