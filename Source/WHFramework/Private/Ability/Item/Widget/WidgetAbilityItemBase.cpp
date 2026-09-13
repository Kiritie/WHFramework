// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Item/Widget/WidgetAbilityItemBase.h"

#include "Widget/WidgetModuleStatics.h"
#include "Ability/Item/AbilityItemDataBase.h"

UWidgetAbilityItemBase::UWidgetAbilityItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
	
	Item = FAbilityItem::Empty;
}

void UWidgetAbilityItemBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);
}

void UWidgetAbilityItemBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);
}

void UWidgetAbilityItemBase::OnCreate(const FParameter& InParam)
{
	Super::OnCreate(InParam);
}

void UWidgetAbilityItemBase::OnInitialize(const FParameter& InParam)
{
	if(const FAbilityWidgetSpawnParameter* Param = InParam.GetPtr<FAbilityWidgetSpawnParameter>())
	{
		Item = Param->Item;
		Item.Payload = this;
	}
	Super::OnInitialize(InParam);
}

void UWidgetAbilityItemBase::OnRefresh()
{
	Super::OnRefresh();
}

void UWidgetAbilityItemBase::OnDestroy(EObjectDespawnMode InMode)
{
	Super::OnDestroy(InMode);
	
	Item = FAbilityItem::Empty;
}
