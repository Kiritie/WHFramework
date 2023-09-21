// Fill out your copyright notice in the Description page of Project Settings.

#include "Scene/Components/WorldTimerComponent.h"

#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"

UWorldTimerComponent::UWorldTimerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	SkyLight = nullptr;
	SunLight = nullptr;

	TimeSeconds = -1.f;
	SecondsOfDay = 300.f;
	CurrentDay = 0;
	CurrentHour = 0;
	CurrentMinute = 0;
	CurrentSeconds = 0;
}

void UWorldTimerComponent::BeginPlay()
{
	Super::BeginPlay();

	UpdateTimer();
}

void UWorldTimerComponent::InitializeTimer(float InSecondsOfDay, float InTimeSeconds)
{
	SecondsOfDay = InSecondsOfDay;
	SetCurrentTime(InTimeSeconds);
}

void UWorldTimerComponent::UpdateTimer(float DeltaSeconds)
{
	TimeSeconds += DeltaSeconds;

	#if ENGINE_MAJOR_VERSION == 4
	float RemainSeconds = 0.f;
	#else if ENGINE_MAJOR_VERSION == 5
	double RemainSeconds = 0.f;
	#endif
	
	CurrentDay = UKismetMathLibrary::FMod(TimeSeconds, SecondsOfDay, RemainSeconds);

	UpdateLight((RemainSeconds / SecondsOfDay) * 360.f + 90.f);

	CurrentHour = UKismetMathLibrary::FMod(RemainSeconds, SecondsOfDay / 24, RemainSeconds);
	CurrentMinute = UKismetMathLibrary::FMod(RemainSeconds, SecondsOfDay / 24 / 60, RemainSeconds);
	CurrentSeconds = UKismetMathLibrary::FMod(RemainSeconds, SecondsOfDay / 24 / 60 / 60, RemainSeconds);

	TimeSeconds += DeltaSeconds;
}

void UWorldTimerComponent::SetCurrentTime(float InTimeSeconds, bool bUpdateTimer /*= true*/)
{
	TimeSeconds = InTimeSeconds != -1.f ? InTimeSeconds : GetSunriseTime();
	if (bUpdateTimer)
	{
		UpdateTimer();
	}
}

float UWorldTimerComponent::GetSunriseTime() const
{
	return (SecondsOfDay / 4) * 1.5f;
}

float UWorldTimerComponent::GetNoonTime() const
{
	return (SecondsOfDay / 4) * 2.f;
}

float UWorldTimerComponent::GetSunsetTime() const
{
	return (SecondsOfDay / 4) * 3.5f;
}
