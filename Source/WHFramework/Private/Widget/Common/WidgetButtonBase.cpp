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

void UWidgetButtonBase::NativePreConstruct()
{
	Super::NativePreConstruct();

	if(Txt_Title)
	{
		Txt_Title->SetText(Title);
	}
}

void UWidgetButtonBase::NativeOnCurrentTextStyleChanged()
{
	Super::NativeOnCurrentTextStyleChanged();

	if(Txt_Title)
	{
		Txt_Title->SetStyle(GetCurrentTextStyleClass());
	}
}

void UWidgetButtonBase::SetTitle(const FText InTitle)
{
	Title = InTitle;
	if(Txt_Title)
	{
		Txt_Title->SetText(Title);
	}
}

void UWidgetButtonBase::SetMinWidth(int32 InValue)
{
	MinWidth = InValue;
}

void UWidgetButtonBase::SetMinHeight(int32 InValue)
{
	MinHeight = InValue;
}
