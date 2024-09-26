// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Prop/AbilityPropBase.h"

AAbilityPropBase::AAbilityPropBase()
{
}

void AAbilityPropBase::Initialize_Implementation(AActor* InOwnerActor, const FAbilityItem& InItem)
{
	Super::Initialize_Implementation(InOwnerActor, InItem);
}
