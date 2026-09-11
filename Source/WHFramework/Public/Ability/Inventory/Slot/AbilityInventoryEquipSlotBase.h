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

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:

	virtual void OnSpawn_Implementation(
		const FParameter& InParameter) override;
		
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;

public:
	virtual void OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex) override;

	virtual void OnItemPreChange(FAbilityItem& InNewItem, bool bBroadcast) override;

	virtual void OnItemChanged(FAbilityItem& InOldItem, bool bBroadcast) override;

	virtual bool MatchItemLimit(FAbilityItem InItem, bool bForce = false) const override;
	
	virtual void Refresh() override;
};
