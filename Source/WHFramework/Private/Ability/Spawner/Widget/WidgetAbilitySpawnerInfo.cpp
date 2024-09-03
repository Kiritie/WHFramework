// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Spawner/Widget/WidgetAbilitySpawnerInfo.h"

UWidgetAbilitySpawnerInfo::UWidgetAbilitySpawnerInfo(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetName = FName("SpawnerInfo");

	WidgetSpace = EWidgetSpace::World;
	WidgetAlignment = FVector2D(0.5f);
	bWidgetAutoSize = true;
}

void UWidgetAbilitySpawnerInfo::InitAbilityItem_Implementation(const FAbilityItem& InAbilityItem)
{
}
