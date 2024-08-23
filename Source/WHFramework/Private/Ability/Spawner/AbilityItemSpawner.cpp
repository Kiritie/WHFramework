// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Spawner/AbilityItemSpawner.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/AbilityModuleTypes.h"
#include "Ability/Item/AbilityItemBase.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextRenderComponent.h"

AAbilityItemSpawner::AAbilityItemSpawner()
{
	GetCapsuleComponent()->InitCapsuleSize(40.0f, 40.0f);
}

void AAbilityItemSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITORONLY_DATA
	const auto& ItemData = AbilityItem.GetData<UAbilityItemDataBase>(false);

	TextComponent->SetText(ItemData.Name);
#endif
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
