// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Item/Widget/WidgetAbilityItemBase.h"

#include "Widget/WidgetModuleBPLibrary.h"
#include "Ability/Item/AbilityItemDataBase.h"

UWidgetAbilityItemBase::UWidgetAbilityItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
	
	Item = FAbilityItem::Empty;
}

void UWidgetAbilityItemBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void UWidgetAbilityItemBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
	
	Item = FAbilityItem::Empty;
}

void UWidgetAbilityItemBase::OnCreate_Implementation(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate_Implementation(InOwner, InParams);
}

void UWidgetAbilityItemBase::OnInitialize_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Item = InParams[0].GetPointerValueRef<FAbilityItem>();
	}
	Super::OnInitialize_Implementation(InParams);
}

void UWidgetAbilityItemBase::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
}

void UWidgetAbilityItemBase::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
