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

void UWidgetAbilityItemBase::OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);
}

void UWidgetAbilityItemBase::OnInitialize(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Item = InParams[0].Get<FAbilityItem>();
		Item.Payload = this;
	}
	Super::OnInitialize(InParams);
}

void UWidgetAbilityItemBase::OnRefresh()
{
	Super::OnRefresh();
}

void UWidgetAbilityItemBase::OnDestroy(bool bRecovery)
{
	Super::OnDestroy(bRecovery);
	
	Item = FAbilityItem::Empty;
}
