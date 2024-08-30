// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Effects/EffectBase.h"

void UEffectBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
}

void UEffectBase::OnDespawn_Implementation(bool bRecovery)
{
	Modifiers.Empty();
}
