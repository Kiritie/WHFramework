// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonTextBlockN.h"

UCommonTextBlockN::UCommonTextBlockN(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<UCommonTextStyle> StyleClassFinder(TEXT("/Script/Engine.Blueprint'/WHFramework/Widget/Blueprints/Common/_Style/Text/CTS_Default.CTS_Default_C'"));
	if(StyleClassFinder.Succeeded())
	{
		SetStyle(StyleClassFinder.Class);
	}
}

void UCommonTextBlockN::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void UCommonTextBlockN::OnDespawn_Implementation(bool bRecovery)
{
	SetText(FText::GetEmpty());

	RemoveFromParent();
}
