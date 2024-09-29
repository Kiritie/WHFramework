// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonButton.h"

#include "Components/Image.h"
#include "Widget/Common/CommonTextBlockN.h"

UCommonButton::UCommonButton(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Img_Icon = nullptr;
	Txt_Title = nullptr;

	bStandalone = false;

	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> StyleClassFinder(TEXT("/Script/Engine.Blueprint'/WHFramework/Widget/Blueprints/Common/_Style/Button/CBS_Default.CBS_Default_C'"));
	if(StyleClassFinder.Succeeded())
	{
		Style = StyleClassFinder.Class;
	}
}

void UCommonButton::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void UCommonButton::OnDespawn_Implementation(bool bRecovery)
{
	SetTitle(FText::GetEmpty());

	OnSelectedChangedBase.Clear();
	OnButtonBaseClicked.Clear();
	OnButtonBaseDoubleClicked.Clear();
	OnButtonBaseHovered.Clear();
	OnButtonBaseUnhovered.Clear();

	if(GetSelected())
	{
		SetSelectedInternal(false, false, false);
	}

	HoldReset();

	RemoveFromParent();
}

void UCommonButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	SetTitle(Title);
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
}

void UCommonButton::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);
}

void UCommonButton::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);
}

void UCommonButton::SetTitle(const FText InTitle)
{
	Title = InTitle;
	if(Txt_Title)
	{
		Txt_Title->SetText(Title);
		Txt_Title->SetVisibility(Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UCommonButton::SetIconBrush(const FSlateBrush& InBrush)
{
	if(Img_Icon)
	{
		Img_Icon->SetBrush(InBrush);
		Img_Icon->SetVisibility(!InBrush.HasUObject() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
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
