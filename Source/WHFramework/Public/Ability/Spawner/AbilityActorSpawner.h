// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySpawnerBase.h"
#include "AbilityActorSpawner.generated.h"

UCLASS()
class WHFRAMEWORK_API AAbilityActorSpawner : public AAbilitySpawnerBase
{
	GENERATED_BODY()

public:
	AAbilityActorSpawner();

protected:
	virtual AActor* SpawnImpl_Implementation(const FAbilityItem& InAbilityItem) override;

	virtual void DestroyImpl_Implementation(AActor* InAbilityActor) override;
};
