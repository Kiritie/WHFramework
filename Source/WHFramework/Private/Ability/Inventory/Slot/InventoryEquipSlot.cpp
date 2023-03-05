// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Inventory/Slot/InventoryEquipSlot.h"

UInventoryEquipSlot::UInventoryEquipSlot()
{
}

void UInventoryEquipSlot::OnInitialize(UInventory* InInventory, FAbilityItem InItem, EAbilityItemType InLimitType /* = EAbilityItemType::None */, ESplitSlotType InSplitType /*= ESplitSlotType::Default*/)
{
	Super::OnInitialize(InInventory, InItem, InLimitType, InSplitType);
}

void UInventoryEquipSlot::OnInitialize(UInventory* InInventory, FAbilityItem InItem, EAbilityItemType InLimitType, ESplitSlotType InSplitType, int32 InPartType)
{
	Super::OnInitialize(InInventory, InItem, InLimitType, InSplitType);
}

void UInventoryEquipSlot::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void UInventoryEquipSlot::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
}

bool UInventoryEquipSlot::CheckSlot(FAbilityItem& InItem) const
{
	return Super::CheckSlot(InItem);
}

void UInventoryEquipSlot::Refresh()
{
	Super::Refresh();
}

void UInventoryEquipSlot::OnItemPreChange(FAbilityItem& InNewItem)
{
	Super::OnItemPreChange(InNewItem);
	CancelItem(true);
}

void UInventoryEquipSlot::OnItemChanged(FAbilityItem& InOldItem)
{
	Super::OnItemChanged(InOldItem);
	ActiveItem(true);
}
