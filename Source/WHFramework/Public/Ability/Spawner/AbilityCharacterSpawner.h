// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilityActorSpawner.h"
#include "AbilityCharacterSpawner.generated.h"

UCLASS()
class WHFRAMEWORK_API AAbilityCharacterSpawner : public AAbilityActorSpawner
{
	GENERATED_BODY()

public:
	AAbilityCharacterSpawner();

protected:
	virtual AActor* SpawnImpl_Implementation(const FAbilityItem& InAbilityItem) override;

	virtual void DestroyImpl_Implementation(AActor* InAbilityActor) override;
};
