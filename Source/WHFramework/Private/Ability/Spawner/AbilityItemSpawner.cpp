// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Spawner/AbilityItemSpawner.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/AbilityModuleTypes.h"
#include "Ability/Item/AbilityItemBase.h"
#include "Components/CapsuleComponent.h"

AAbilityItemSpawner::AAbilityItemSpawner()
{
	GetCapsuleComponent()->InitCapsuleSize(40.0f, 40.0f);
}

AActor* AAbilityItemSpawner::SpawnImpl_Implementation(const FAbilityItem& InAbilityItem)
{
	AAbilityItemBase* Item = UAbilityModuleStatics::SpawnAbilityItem(InAbilityItem, GetActorLocation(), GetActorRotation());

	return Item;
}

void AAbilityItemSpawner::DestroyImpl_Implementation(AActor* InAbilityActor)
{
	Super::DestroyImpl_Implementation(InAbilityActor);
}
