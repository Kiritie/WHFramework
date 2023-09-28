// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityInventorySlot.h"
#include "AbilityInventorySkillSlot.generated.h"

/**
 * ���ܲ�
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityInventorySkillSlot : public UAbilityInventorySlot
{
	GENERATED_BODY()

public:
	UAbilityInventorySkillSlot();

public:
	virtual void OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex) override;

	virtual void OnItemPreChange(FAbilityItem& InNewItem) override;

	virtual void OnItemChanged(FAbilityItem& InOldItem) override;
	
	virtual bool ActiveItem(bool bPassive = false) override;
	
	virtual void CancelItem(bool bPassive = false) override;
};
