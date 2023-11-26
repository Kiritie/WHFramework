// Fill out your copyright notice in the Description page of Project Settings.

#include "Common/Base/WHObject.h"

#include "Debug/DebugModuleTypes.h"

UWHObject::UWHObject()
{
	
}

void UWHObject::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	
}

void UWHObject::OnDespawn_Implementation(bool bRecovery)
{
	
}

UWorld* UWHObject::GetWorld() const
{
	if (!HasAnyFlags(RF_ClassDefaultObject) && ensureEditorMsgf(GetOuter(), FString::Printf(TEXT("UWHObject: %s has a null OuterPrivate in UWHObject::GetWorld()"), *GetFullName()), EDC_Default, EDV_Error)
		&& !GetOuter()->HasAnyFlags(RF_BeginDestroyed) && !GetOuter()->IsUnreachable())
	{
		return GetOuter()->GetWorld();
	}
	return nullptr;
}
