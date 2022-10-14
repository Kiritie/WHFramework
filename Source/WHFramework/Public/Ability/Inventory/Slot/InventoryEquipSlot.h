// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "InventorySlot.h"
#include "InventoryEquipSlot.generated.h"

/**
 * װ����
 */
UCLASS()
class WHFRAMEWORK_API UInventoryEquipSlot : public UInventorySlot
{
	GENERATED_BODY()

public:
	UInventoryEquipSlot();

public:
	virtual void OnInitialize(UInventory* InInventory, FAbilityItem InItem, EAbilityItemType InLimitType , ESplitSlotType InSplitType) override;

	virtual void OnInitialize(UInventory* InInventory, FAbilityItem InItem, EAbilityItemType InLimitType , ESplitSlotType InSplitType, int32 InPartType);

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation() override;

	virtual bool CheckSlot(FAbilityItem& InItem) const override;
	
	virtual void Refresh() override;

	virtual void OnItemPreChange(FAbilityItem& InNewItem) override;

	virtual void OnItemChanged(FAbilityItem& InOldItem) override;
};
