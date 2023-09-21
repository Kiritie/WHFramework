// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Character/AbilityCharacterPartBase.h"

#include "Ability/Character/AbilityCharacterBase.h"

UAbilityCharacterPartBase::UAbilityCharacterPartBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UAbilityCharacterPartBase::BeginPlay()
{
	Super::BeginPlay();
}

void UAbilityCharacterPartBase::UpdateVoxelOverlap()
{
	const auto OwnerCharacter = Cast<AAbilityCharacterBase>(GetOwnerCharacter());
	if(!OwnerCharacter || !OwnerCharacter->IsActive()) return;

	Super::UpdateVoxelOverlap();
}
