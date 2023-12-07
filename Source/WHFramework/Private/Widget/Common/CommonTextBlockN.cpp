// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonTextBlockN.h"

UCommonTextBlockN::UCommonTextBlockN(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetStyle(UCommonTextStyle::StaticClass());
}

void UCommonTextBlockN::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void UCommonTextBlockN::OnDespawn_Implementation(bool bRecovery)
{
	SetText(FText::GetEmpty());

	RemoveFromParent();
}
