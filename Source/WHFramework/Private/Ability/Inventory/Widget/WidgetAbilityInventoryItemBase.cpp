// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Inventory/Widget/WidgetAbilityInventoryItemBase.h"
#include "Widget/WidgetModuleBPLibrary.h"
#include "Ability/Item/AbilityItemDataBase.h"

UWidgetAbilityInventoryItemBase::UWidgetAbilityInventoryItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
	
	Item = FAbilityItem::Empty;
}

void UWidgetAbilityInventoryItemBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void UWidgetAbilityInventoryItemBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
	
	Item = FAbilityItem::Empty;
}

void UWidgetAbilityInventoryItemBase::OnCreate_Implementation(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate_Implementation(InOwner, InParams);
}

void UWidgetAbilityInventoryItemBase::OnInitialize_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Item = InParams[0].GetPointerValueRef<FAbilityItem>();
	}
	Super::OnInitialize_Implementation(InParams);
}

void UWidgetAbilityInventoryItemBase::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
}

void UWidgetAbilityInventoryItemBase::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}

FReply UWidgetAbilityInventoryItemBase::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	OnSelected();
	return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

FReply UWidgetAbilityInventoryItemBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UWidgetAbilityInventoryItemBase::OnSelected_Implementation()
{
	
}

void UWidgetAbilityInventoryItemBase::OnUnSelected_Implementation()
{
	
}
