// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/WidgetButtonBase.h"
#include "CommonTextBlock.h"

void UWidgetButtonBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	IObjectPoolInterface::OnSpawn_Implementation(InParams);
}

void UWidgetButtonBase::OnDespawn_Implementation(bool bRecovery)
{
	IObjectPoolInterface::OnDespawn_Implementation(bRecovery);
}

void UWidgetButtonBase::NativeOnCurrentTextStyleChanged()
{
	Super::NativeOnCurrentTextStyleChanged();

	if(Txt_Title)
	{
		Txt_Title->SetStyle(GetCurrentTextStyleClass());
	}
}

void UWidgetButtonBase::SetButtonText(const FText InButtonText)
{
	ButtonText = InButtonText;
	if(Txt_Title)
	{
		Txt_Title->SetText(ButtonText);
	}
}
