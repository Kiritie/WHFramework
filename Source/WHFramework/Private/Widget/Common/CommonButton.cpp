// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonButton.h"
#include "CommonTextBlock.h"

UCommonButton::UCommonButton(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Txt_Title = nullptr;
	bClicked = false;
}

void UCommonButton::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	
}

void UCommonButton::OnDespawn_Implementation(bool bRecovery)
{
	SetIsSelected(false);
	SetTitle(FText::GetEmpty());
	bClicked = false;
}

void UCommonButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	if(Txt_Title)
	{
		Txt_Title->SetText(Title);
	}
}

void UCommonButton::NativeOnCurrentTextStyleChanged()
{
	Super::NativeOnCurrentTextStyleChanged();

	if(Txt_Title)
	{
		Txt_Title->SetStyle(GetCurrentTextStyleClass());
	}
}

void UCommonButton::NativeOnClicked()
{
	Super::NativeOnClicked();

	bClicked = true;
}

void UCommonButton::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);

	bClicked = false;
}

void UCommonButton::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);

	bClicked = false;
}

void UCommonButton::SetTitle(const FText InTitle)
{
	Title = InTitle;
	if(Txt_Title)
	{
		Txt_Title->SetText(Title);
	}
}

void UCommonButton::SetMinWidth(int32 InValue)
{
	MinWidth = InValue;
}

void UCommonButton::SetMinHeight(int32 InValue)
{
	MinHeight = InValue;
}
