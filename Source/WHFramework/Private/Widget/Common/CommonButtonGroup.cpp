// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonButtonGroup.h"

#include "ObjectPool/ObjectPoolModuleStatics.h"

UCommonButtonGroup::UCommonButtonGroup()
{
	
}

void UCommonButtonGroup::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void UCommonButtonGroup::OnDespawn_Implementation(bool bRecovery)
{
	ForEach([](UCommonButtonBase& InButton, int32 InIndex)
	{
		UObjectPoolModuleStatics::DespawnObject(&InButton, true);
	});
	RemoveAll();
}
