// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonTabList.h"

UCommonTabList::UCommonTabList(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	
}

void UCommonTabList::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void UCommonTabList::OnDespawn_Implementation(bool bRecovery)
{
	RemoveFromParent();
}
