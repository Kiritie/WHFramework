// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Inventory/Slot/AbilityInventoryEquipSlotBase.h"

#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Item/Equip/AbilityEquipDataBase.h"

UAbilityInventoryEquipSlotBase::UAbilityInventoryEquipSlotBase()
{
}

void UAbilityInventoryEquipSlotBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void UAbilityInventoryEquipSlotBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
}

void UAbilityInventoryEquipSlotBase::OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex)
{
	Super::OnInitialize(InInventory, InLimitType, InSplitType, InSlotIndex);
}

void UAbilityInventoryEquipSlotBase::OnItemPreChange(FAbilityItem& InNewItem, bool bBroadcast)
{
	Super::OnItemPreChange(InNewItem, bBroadcast);

	if(Item.IsValid() && Item.GetData<UAbilityEquipDataBase>().EquipMode == EAbilityEquipMode::Passive)
	{
		DeactiveItem(true);
	}
}

void UAbilityInventoryEquipSlotBase::OnItemChanged(FAbilityItem& InOldItem, bool bBroadcast)
{
	Super::OnItemChanged(InOldItem, bBroadcast);

	if(Item.IsValid() && Item.GetData<UAbilityEquipDataBase>().EquipMode == EAbilityEquipMode::Passive)
	{
		ActiveItem(true);
	}
}

bool UAbilityInventoryEquipSlotBase::MatchItemLimit(FAbilityItem InItem, bool bForce) const
{
	return Super::MatchItemLimit(InItem, bForce);
}

void UAbilityInventoryEquipSlotBase::Refresh()
{
	Super::Refresh();
}
