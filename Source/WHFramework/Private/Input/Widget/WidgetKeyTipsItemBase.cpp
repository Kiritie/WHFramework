// Fill out your copyright notice in the Description page of Project Settings.

#include "Input/Widget/WidgetKeyTipsItemBase.h"

#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Input/Widget/WidgetInputAction.h"
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
	if(!Box_KeyIcon)
	{
		return;
	}

	Box_KeyIcon->ClearChildren();
	for(const FGameplayTag& ActionTag : ActionTags)
	{
		if(!ActionTag.IsValid())
		{
			continue;
		}

		UWidgetInputAction* ActionWidget = NewObject<UWidgetInputAction>(this);
		ActionWidget->SetActionTag(ActionTag);
		Box_KeyIcon->AddChildToHorizontalBox(ActionWidget);
	}

	Box_KeyIcon->SetVisibility(Box_KeyIcon->GetChildrenCount() > 0
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);
	if(Border_KeyCode)
	{
		Border_KeyCode->SetVisibility(ESlateVisibility::Collapsed);
	}
	if(Txt_KeyCode)
	{
		Txt_KeyCode->SetText(FText::GetEmpty());
		Txt_KeyCode->SetVisibility(ESlateVisibility::Collapsed);
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
