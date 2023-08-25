// Fill out your copyright notice in the Description page of Project Settings.

#include "Global/Base/WHObject.h"

UWHObject::UWHObject()
{
	Execute_OnReset(this);
}

void UWHObject::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	
}

void UWHObject::OnDespawn_Implementation(bool bRecovery)
{
	
}

void UWHObject::OnReset_Implementation()
{
}
