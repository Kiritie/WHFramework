// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Raw/AbilityRawBase.h"

AAbilityRawBase::AAbilityRawBase()
{
}

void AAbilityRawBase::Initialize_Implementation(AActor* InOwnerActor, const FAbilityItem& InItem)
{
	Super::Initialize_Implementation(InOwnerActor, InItem);
}
