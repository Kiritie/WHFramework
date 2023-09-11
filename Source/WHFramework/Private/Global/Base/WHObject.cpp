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

UWorld* UWHObject::GetWorld() const
{
	if (!HasAnyFlags(RF_ClassDefaultObject) && ensureMsgf(GetOuter(), TEXT("UWHObject: %s has a null OuterPrivate in UWHObject::GetWorld()"), *GetFullName())
		&& !GetOuter()->HasAnyFlags(RF_BeginDestroyed) && !GetOuter()->IsUnreachable())
	{
		if (const ULevel* Level = GetTypedOuter<ULevel>())
		{
			return Level->OwningWorld;
		}
	}
	return nullptr;
}
