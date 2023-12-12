// Fill out your copyright notice in the Description page of Project Settings.

#include "Scene/Object/WorldTimer.h"

#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"

UWorldTimer::UWorldTimer()
{

}

void UWorldTimer::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void UWorldTimer::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
}

void UWorldTimer::ResetDateTime_Implementation(float InNewTime) const
{
	const float NewTime = InNewTime == -1.f ? GetTimeOfDay() : InNewTime;
	SetDateTime(FDateTime());
	SetTimeOfDay(NewTime);
}
