// Fill out your copyright notice in the Description page of Project Settings.

#include "Input/Widget/WidgetKeyTipsItemBase.h"

#include "CommonInputBaseTypes.h"
#include "CommonInputSubsystem.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Input/InputModuleStatics.h"
#include "Widget/Common/CommonTextBlockN.h"

UWidgetKeyTipsItemBase::UWidgetKeyTipsItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Box_KeyIcon = nullptr;
	Border_KeyCode = nullptr;
	Txt_KeyCode = nullptr;
	Txt_DisplayName = nullptr;
}

void UWidgetKeyTipsItemBase::NativePreConstruct()
{
	Super::NativePreConstruct();

	SetKeyDisplayName(KeyDisplayName);
	RefreshData();
}

void UWidgetKeyTipsItemBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);

	RefreshData();
}

void UWidgetKeyTipsItemBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);
}

void UWidgetKeyTipsItemBase::RefreshData_Implementation()
{
	UImage* ImageTemplate = Box_KeyIcon && Box_KeyIcon->GetChildrenCount() > 0
		? Cast<UImage>(Box_KeyIcon->GetChildAt(0))
		: nullptr;
	if(!ImageTemplate)
	{
		return;
	}

	while(Box_KeyIcon->GetChildrenCount() > 1)
	{
		Box_KeyIcon->RemoveChildAt(1);
	}

	FString KeyCode;
	const UCommonInputPlatformSettings* Settings = UPlatformSettingsManager::Get().GetSettingsForPlatform<UCommonInputPlatformSettings>();
	ECommonInputType InputType;
	FName GamepadName;
	FCommonInputBase::GetCurrentPlatformDefaults(InputType, GamepadName);
	if(const UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		InputType = InputSubsystem->GetCurrentInputType();
		GamepadName = InputSubsystem->GetCurrentGamepadName();
	}
	for(const FGameplayTag& ActionTag : ActionTags)
	{
		if(!ActionTag.IsValid())
		{
			continue;
		}

		for(const FKey& Key : UInputModuleStatics::GetKeysByActionTag(ActionTag))
		{
			FSlateBrush ImageBrush;
			if(Settings->TryGetInputBrush(
				ImageBrush,
				Key,
				InputType,
				GamepadName)
				&& ImageBrush.GetResourceObject())
			{
				ImageBrush.ImageSize = ImageTemplate->GetBrush().ImageSize;
				UImage* Image = NewObject<UImage>(this);
				Image->SetBrush(ImageBrush);
				if(UHorizontalBoxSlot* ImageSlot = Box_KeyIcon->AddChildToHorizontalBox(Image))
				{
					ImageSlot->SetPadding(CastChecked<UHorizontalBoxSlot>(ImageTemplate->Slot)->GetPadding());
					ImageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				}
			}
			else
			{
				if(!KeyCode.IsEmpty())
				{
					KeyCode += TEXT("/");
				}
				KeyCode += Key.GetDisplayName(false).ToString();
			}
		}
	}

	if(Box_KeyIcon->GetChildrenCount() > 1)
	{
		CastChecked<UHorizontalBoxSlot>(Box_KeyIcon->GetChildAt(Box_KeyIcon->GetChildrenCount() - 1)->Slot)->SetPadding(FMargin(0.f));
	}
	Box_KeyIcon->SetVisibility(Box_KeyIcon->GetChildrenCount() > 1
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);
	if(Border_KeyCode)
	{
		Border_KeyCode->SetVisibility(KeyCode.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
	if(Txt_KeyCode)
	{
		Txt_KeyCode->SetText(FText::FromString(KeyCode));
		Txt_KeyCode->SetVisibility(KeyCode.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
}

FText UWidgetKeyTipsItemBase::GetKeyDisplayName() const
{
	return KeyDisplayName;
}

void UWidgetKeyTipsItemBase::SetKeyDisplayName(const FText InKeyDisplayName)
{
	KeyDisplayName = InKeyDisplayName;

	if(Txt_DisplayName)
	{
		Txt_DisplayName->SetText(KeyDisplayName);
		Txt_DisplayName->SetVisibility(KeyDisplayName.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UWidgetKeyTipsItemBase::SetActionTags(const TArray<FGameplayTag>& InActionTags)
{
	ActionTags = InActionTags;
	RefreshData();
}
