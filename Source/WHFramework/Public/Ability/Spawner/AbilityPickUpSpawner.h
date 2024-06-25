// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilityActorSpawner.h"
#include "AbilityPickUpSpawner.generated.h"

UCLASS()
class WHFRAMEWORK_API AAbilityPickUpSpawner : public AAbilityActorSpawner
{
	GENERATED_BODY()

public:
	AAbilityPickUpSpawner();

protected:
	virtual AActor* SpawnImpl_Implementation(const FAbilityItem& InAbilityItem) override;

	virtual void DestroyImpl_Implementation(AActor* InAbilityActor) override;
};
