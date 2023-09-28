// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityInventorySlot.h"
#include "AbilityInventoryEquipSlot.generated.h"

/**
 * װ����
 */
UCLASS()
class WHFRAMEWORK_API UAbilityInventoryEquipSlot : public UAbilityInventorySlot
{
	GENERATED_BODY()

public:
	UAbilityInventoryEquipSlot();

public:
	virtual void OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex) override;

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual bool CheckSlot(FAbilityItem& InItem) const override;
	
	virtual void Refresh() override;

	virtual void OnItemPreChange(FAbilityItem& InNewItem) override;

	virtual void OnItemChanged(FAbilityItem& InOldItem) override;
};
