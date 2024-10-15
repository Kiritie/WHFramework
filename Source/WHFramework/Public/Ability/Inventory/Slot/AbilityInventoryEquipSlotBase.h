// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityInventorySlotBase.h"
#include "AbilityInventoryEquipSlotBase.generated.h"

/**
 * װ����
 */
UCLASS()
class WHFRAMEWORK_API UAbilityInventoryEquipSlotBase : public UAbilityInventorySlotBase
{
	GENERATED_BODY()

public:
	UAbilityInventoryEquipSlotBase();

public:
	virtual void OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex) override;

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual bool CheckSlot(FAbilityItem& InItem) const override;
	
	virtual void Refresh() override;

	virtual void OnItemPreChange(FAbilityItem& InNewItem) override;

	virtual void OnItemChanged(FAbilityItem& InOldItem) override;
};
