// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/PickUp/Widget/WidgetAbilityPickUpSprite.h"

UWidgetAbilityPickUpSprite::UWidgetAbilityPickUpSprite(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

	WidgetSpace = EWidgetSpace::World;
	WidgetAlignment = FVector2D(0.5f);
	bWidgetAutoSize = true;
}

void UWidgetAbilityPickUpSprite::InitAbilityItem_Implementation(const FAbilityItem& InAbilityItem)
{
}
