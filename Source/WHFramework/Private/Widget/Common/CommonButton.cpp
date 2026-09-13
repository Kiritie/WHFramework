// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonButton.h"

#include "Components/Image.h"
#include "Input/Base/InputActionBase.h"
#include "Input/InputModule.h"
#include "Widget/Common/CommonTextBlockN.h"
#include "Widget/Theme/WidgetTheme.h"
#include "Widget/WidgetModule.h"

UCommonButton::UCommonButton(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Img_Icon = nullptr;
	Txt_Title = nullptr;

	bStandalone = false;
	WidgetParams = FParameter();

}

void UCommonButton::OnSpawn_Implementation(const FParameter& InParam)
{
	WidgetParams = InParam;
}

void UCommonButton::OnDespawn_Implementation(EObjectDespawnMode InMode)
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
	WidgetParams.Reset();
}

void UCommonButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplyStyleTag();
	ApplyTriggeringActionTag();
	SetTitle(Title);
}

void UCommonButton::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	if (!InDragDropEvent.IsTouchEvent())
	{
		if (GetIsEnabled() && IsInteractionEnabled())
		{
			NativeOnHovered();
		}
	}
}

void UCommonButton::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);

	if (!InDragDropEvent.IsTouchEvent())
	{
		if (GetIsEnabled() && IsInteractionEnabled())
		{
			NativeOnUnhovered();
		}
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

void UCommonButton::SetIsEnabledN(bool bEnable)
{
	SetRenderOpacity(bEnable ? 1.f : 0.5f);
	SetVisibility(bEnable ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::HitTestInvisible);
}

void UCommonButton::SetStyleTag(FGameplayTag InStyleTag)
{
	StyleTag = InStyleTag;
	ApplyStyleTag();
}

void UCommonButton::SetTriggeringActionTag(FGameplayTag InActionTag)
{
	TriggeringActionTag = InActionTag;
	ApplyTriggeringActionTag();
}

void UCommonButton::ApplyStyleTag()
{
	const bool bInEditor = IsDesignTime();
	const UWidgetModule* WidgetModule = UWidgetModule::IsValid(bInEditor)
		? UWidgetModule::GetPtr(bInEditor)
		: nullptr;
	const UWidgetTheme* Theme = WidgetModule ? WidgetModule->GetDefaultWidgetTheme() : nullptr;
	const FWidgetButtonStyleData* StyleData = Theme ? Theme->FindButtonStyle(StyleTag) : nullptr;
	if(StyleData && StyleData->Style)
	{
		SetStyle(StyleData->Style);
	}
}

void UCommonButton::ApplyTriggeringActionTag()
{
	const UInputActionBase* InputAction = TriggeringActionTag.IsValid()
		? UInputModule::Get().GetInputActionByTag(TriggeringActionTag, false)
		: nullptr;
	SetTriggeringEnhancedInputAction(const_cast<UInputActionBase*>(InputAction));
}
