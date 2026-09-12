// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Effects/EffectBase.h"

void UEffectBase::OnSpawn_Implementation(const FParameter& InParam)
{
}

void UEffectBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Modifiers.Empty();
}
