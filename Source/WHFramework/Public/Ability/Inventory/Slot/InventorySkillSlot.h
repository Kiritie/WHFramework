// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "InventorySlot.h"
#include "InventorySkillSlot.generated.h"

/**
 * ���ܲ�
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UInventorySkillSlot : public UInventorySlot
{
	GENERATED_BODY()

public:
	UInventorySkillSlot();

public:
	virtual void OnInitialize(UInventory* InInventory, FAbilityItem InItem, EAbilityItemType InLimitType /* = EAbilityItemType::None */, ESplitSlotType InSplitType /*= ESplitSlotType::Default*/) override;

	virtual void OnItemPreChange(FAbilityItem& InNewItem) override;

	virtual void OnItemChanged(FAbilityItem& InOldItem) override;
	
	virtual bool ActiveItem(bool bPassive = false) override;
	
	virtual void CancelItem(bool bPassive = false) override;
};
