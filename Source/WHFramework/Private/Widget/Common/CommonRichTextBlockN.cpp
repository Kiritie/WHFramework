// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonRichTextBlockN.h"

UCommonRichTextBlockN::UCommonRichTextBlockN(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	
}

void UCommonRichTextBlockN::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void UCommonRichTextBlockN::OnDespawn_Implementation(bool bRecovery)
{
	SetText(FText::GetEmpty());

	RemoveFromParent();
}
