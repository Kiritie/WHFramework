// Fill out your copyright notice in the Description Item of Project Settings.

#include "Setting/Widget/Item/WidgetInputSettingItemBase.h"

#include "CommonTextBlock.h"
#include "Components/EditableTextBox.h"

UWidgetInputSettingItemBase::UWidgetInputSettingItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UWidgetInputSettingItemBase::NativeOnCurrentTextStyleChanged()
{
	Super::NativeOnCurrentTextStyleChanged();

	if(!TxtBox_Value)
	{
		return;
	}

	const TSubclassOf<UCommonTextStyle> CurrentTextStyleClass = GetCurrentTextStyleClass();
	if(!CurrentTextStyleClass)
	{
		return;
	}

	const UCommonTextStyle* CurrentTextStyle = CurrentTextStyleClass->GetDefaultObject<UCommonTextStyle>();
	FSlateFontInfo Font;
	FLinearColor Color;
	CurrentTextStyle->GetFont(Font);
	CurrentTextStyle->GetColor(Color);

	FEditableTextBoxStyle WidgetStyle = TxtBox_Value->GetWidgetStyle();
	WidgetStyle.TextStyle.Font = Font;
	WidgetStyle.TextStyle.ColorAndOpacity = FSlateColor(Color);

	TxtBox_Value->WidgetStyle = MoveTemp(WidgetStyle);
	TxtBox_Value->SynchronizeProperties();
}
