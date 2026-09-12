// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonTabList.h"

UCommonTabList::UCommonTabList(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	
}

void UCommonTabList::OnSpawn_Implementation(const FParameter& InParam)
{
	
}

void UCommonTabList::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	RemoveFromParent();
}
