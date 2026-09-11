// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Item/Widget/WidgetAbilityDragItemBase.h"

#include "Widget/WidgetModuleStatics.h"
#include "Ability/Item/AbilityItemDataBase.h"

UWidgetAbilityDragItemBase::UWidgetAbilityDragItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Item = FAbilityItem::Empty;

	UUserWidget::SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UWidgetAbilityDragItemBase::OnSpawn_Implementation(
	const FParameter& InParameter)
{
	Super::OnSpawn_Implementation(InParameter);
	
	if(const FAbilityWidgetSpawnParameter* SpawnParameter = InParameter.GetPtr<FAbilityWidgetSpawnParameter>())
	{
		Item = SpawnParameter->Item;
		Item.Payload = this;
	}
}

void UWidgetAbilityDragItemBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);
	
	Item = FAbilityItem();
}
