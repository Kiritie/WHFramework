// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonButton.h"
#include "CommonTextBlock.h"

UCommonButton::UCommonButton(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Txt_Title = nullptr;
	bClicked = false;

	Style = UCommonButtonStyle::StaticClass();
}

void UCommonButton::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void UCommonButton::OnDespawn_Implementation(bool bRecovery)
{
	bClicked = true;
	SetIsSelected(false);
	SetTitle(FText::GetEmpty());
	bClicked = false;

	RemoveFromParent();
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
