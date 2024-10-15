// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityInventorySlotBase.h"
#include "AbilityInventoryAuxiliarySlotSlot.generated.h"

/**
 * ���ܲ�
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityInventoryAuxiliarySlotSlot : public UAbilityInventorySlotBase
{
	GENERATED_BODY()

public:
	UAbilityInventoryAuxiliarySlotSlot();

public:
	virtual void OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex) override;

	virtual void OnItemChanged(FAbilityItem& InOldItem) override;
};
