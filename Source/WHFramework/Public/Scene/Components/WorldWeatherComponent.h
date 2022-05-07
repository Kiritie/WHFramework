// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "WorldWeatherComponent.generated.h"

class ASkyLight;
class ADirectionalLight;

/**
 * �����������
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class WHFRAMEWORK_API UWorldWeatherComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldWeatherComponent();

protected:
	virtual void BeginPlay() override;

public:
	void UpdateWeather(float DeltaSeconds = 0.f);
};