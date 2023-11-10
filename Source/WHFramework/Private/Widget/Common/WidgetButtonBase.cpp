// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/WidgetButtonBase.h"
#include "CommonTextBlock.h"

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
