// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityInventorySlotBase.h"
#include "AbilityInventorySkillSlotBase.generated.h"

/**
 * ���ܲ�
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityInventorySkillSlotBase : public UAbilityInventorySlotBase
{
	GENERATED_BODY()

public:
	UAbilityInventorySkillSlotBase();

public:
	virtual void OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex) override;

	virtual void OnItemPreChange(FAbilityItem& InNewItem) override;

	virtual void OnItemChanged(FAbilityItem& InOldItem) override;
	
	virtual bool ActiveItem(bool bPassive = false) override;
	
	virtual void DeactiveItem(bool bPassive = false) override;
};
