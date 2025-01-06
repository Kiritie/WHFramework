// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Item/Widget/WidgetAbilityDragItemBase.h"

#include "Widget/WidgetModuleStatics.h"
#include "Ability/Item/AbilityItemDataBase.h"

UWidgetAbilityDragItemBase::UWidgetAbilityDragItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Item = FAbilityItem::Empty;

	UUserWidget::SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UWidgetAbilityDragItemBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
	
	if(InParams.IsValidIndex(0))
	{
		Item = InParams[0].GetPointerValueRef<FAbilityItem>();
	}
}

void UWidgetAbilityDragItemBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
	
	Item = FAbilityItem();
}
