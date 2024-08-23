// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySpawnerBase.h"
#include "AbilityItemSpawner.generated.h"

UCLASS()
class WHFRAMEWORK_API AAbilityItemSpawner : public AAbilitySpawnerBase
{
	GENERATED_BODY()

public:
	AAbilityItemSpawner();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual AActor* SpawnImpl_Implementation(const FAbilityItem& InAbilityItem) override;

	virtual void DestroyImpl_Implementation(AActor* InAbilityActor) override;
};
