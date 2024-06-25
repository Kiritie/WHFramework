// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilityActorSpawner.h"
#include "AbilityVitalitySpawner.generated.h"

UCLASS()
class WHFRAMEWORK_API AAbilityVitalitySpawner : public AAbilityActorSpawner
{
	GENERATED_BODY()

public:
	AAbilityVitalitySpawner();

protected:
	virtual AActor* SpawnImpl_Implementation(const FAbilityItem& InAbilityItem) override;

	virtual void DestroyImpl_Implementation(AActor* InAbilityActor) override;
};
