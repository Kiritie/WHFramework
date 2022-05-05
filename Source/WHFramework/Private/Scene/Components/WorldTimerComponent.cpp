// Fill out your copyright notice in the Description page of Project Settings.

#include "Scene/Components/WorldTimerComponent.h"

#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

UWorldTimerComponent::UWorldTimerComponent()
{
	SkyLight = nullptr;
	SunLight = nullptr;

	SecondsOfDay = 300.f;
	SunriseTime = 6.f;
	SunsetTime = 18.f;
	TimeSeconds = 0.f;
	CurrentDay = 0;
	CurrentHour = 0;
	CurrentMinute = 0;
	CurrentSeconds = 0;
}

void UWorldTimerComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UWorldTimerComponent::UpdateTimer()
{
<<<<<<< HEAD
	TimeSeconds = FMath::TruncToInt(UGameplayStatics::GetTimeSeconds(this));

	float RemainSeconds = 0;
=======
	double RemainSeconds = 0;
>>>>>>> 1adb8b86cec47a2022ebe48c69964589666e1336
	CurrentDay = UKismetMathLibrary::FMod(TimeSeconds, SecondsOfDay, RemainSeconds);

	UpdateLight(RemainSeconds / SecondsOfDay / 24 / 24 * 360 + 200);

	CurrentHour = UKismetMathLibrary::FMod(TimeSeconds, SecondsOfDay / 24, RemainSeconds);
	CurrentMinute = UKismetMathLibrary::FMod(TimeSeconds, SecondsOfDay / 60, RemainSeconds);
	CurrentSeconds = UKismetMathLibrary::FMod(TimeSeconds, SecondsOfDay / 60 / 60, RemainSeconds);

	TimeSeconds += GetWorld()->GetDeltaSeconds();
}

void UWorldTimerComponent::SetTimeSeconds(int InTimeSeconds, bool bUpdateTimer /*= true*/)
{
	TimeSeconds = InTimeSeconds;
	if (bUpdateTimer)
	{
		UpdateTimer();
	}
}
