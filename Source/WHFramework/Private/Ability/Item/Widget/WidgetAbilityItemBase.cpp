// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Item/Widget/WidgetAbilityItemBase.h"

#include "Widget/WidgetModuleStatics.h"
#include "Ability/Item/AbilityItemDataBase.h"

UWidgetAbilityItemBase::UWidgetAbilityItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
	
	Item = FAbilityItem::Empty;
}

void UWidgetAbilityItemBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void UWidgetAbilityItemBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
}

void UWidgetAbilityItemBase::OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);
}

void UWidgetAbilityItemBase::OnInitialize(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Item = InParams[0].GetPointerValueRef<FAbilityItem>();
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
