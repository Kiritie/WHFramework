// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Inventory/Widget/WidgetInventoryItemBase.h"
#include "Widget/WidgetModuleBPLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Global/GlobalBPLibrary.h"

UWidgetInventoryItemBase::UWidgetInventoryItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
	
	Item = FAbilityItem::Empty;
}

void UWidgetInventoryItemBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void UWidgetInventoryItemBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
	
	Item = FAbilityItem::Empty;
}

void UWidgetInventoryItemBase::OnCreate_Implementation(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate_Implementation(InOwner, InParams);
}

void UWidgetInventoryItemBase::OnInitialize_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Item = InParams[0].GetPointerValueRef<FAbilityItem>();
	}
	Super::OnInitialize_Implementation(InParams);
}

void UWidgetInventoryItemBase::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
}

void UWidgetInventoryItemBase::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}

FReply UWidgetInventoryItemBase::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	OnSelected();
	return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

FReply UWidgetInventoryItemBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UWidgetInventoryItemBase::OnSelected_Implementation()
{
	
}

void UWidgetInventoryItemBase::OnUnSelected_Implementation()
{
	
}
