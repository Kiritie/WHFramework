// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Spawner/AbilityPickUpSpawner.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/AbilityModuleTypes.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
#include "Components/CapsuleComponent.h"

AAbilityPickUpSpawner::AAbilityPickUpSpawner()
{
	GetCapsuleComponent()->InitCapsuleSize(40.0f, 40.0f);
}

AActor* AAbilityPickUpSpawner::SpawnImpl_Implementation(const FAbilityItem& InAbilityItem)
{
	return UAbilityModuleStatics::SpawnAbilityPickUp(InAbilityItem, GetActorLocation());
}

void AAbilityPickUpSpawner::DestroyImpl_Implementation(AActor* InAbilityActor)
{
	Super::DestroyImpl_Implementation(InAbilityActor);
}
