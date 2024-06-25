// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilityVitalitySpawner.h"
#include "AbilityPawnSpawner.generated.h"

UCLASS()
class WHFRAMEWORK_API AAbilityPawnSpawner : public AAbilityVitalitySpawner
{
	GENERATED_BODY()

public:
	AAbilityPawnSpawner();

protected:
	virtual AActor* SpawnImpl_Implementation(const FAbilityItem& InAbilityItem) override;

	virtual void DestroyImpl_Implementation(AActor* InAbilityActor) override;
};
