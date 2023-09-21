// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Inventory/Slot/AbilityInventoryEquipSlot.h"

UAbilityInventoryEquipSlot::UAbilityInventoryEquipSlot()
{
}

void UAbilityInventoryEquipSlot::OnInitialize(UAbilityInventoryBase* InInventory, FAbilityItem InItem, EAbilityItemType InLimitType /* = EAbilityItemType::None */, ESplitSlotType InSplitType /*= ESplitSlotType::Default*/)
{
	Super::OnInitialize(InInventory, InItem, InLimitType, InSplitType);
}

void UAbilityInventoryEquipSlot::OnInitialize(UAbilityInventoryBase* InInventory, FAbilityItem InItem, EAbilityItemType InLimitType, ESplitSlotType InSplitType, int32 InPartType)
{
	Super::OnInitialize(InInventory, InItem, InLimitType, InSplitType);
}

void UAbilityInventoryEquipSlot::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void UAbilityInventoryEquipSlot::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
}

bool UAbilityInventoryEquipSlot::CheckSlot(FAbilityItem& InItem) const
{
	return Super::CheckSlot(InItem);
}

void UAbilityInventoryEquipSlot::Refresh()
{
	Super::Refresh();
}

void UAbilityInventoryEquipSlot::OnItemPreChange(FAbilityItem& InNewItem)
{
	Super::OnItemPreChange(InNewItem);
	CancelItem(true);
}

void UAbilityInventoryEquipSlot::OnItemChanged(FAbilityItem& InOldItem)
{
	Super::OnItemChanged(InOldItem);
	ActiveItem(true);
}
