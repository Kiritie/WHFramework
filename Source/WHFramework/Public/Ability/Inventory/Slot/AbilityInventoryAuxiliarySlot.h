// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityInventorySlot.h"
#include "AbilityInventoryAuxiliarySlot.generated.h"

/**
 * ���ܲ�
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityInventoryAuxiliarySlot : public UAbilityInventorySlot
{
	GENERATED_BODY()

public:
	UAbilityInventoryAuxiliarySlot();

public:
	virtual void OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex) override;

	virtual void OnItemChanged(FAbilityItem& InOldItem) override;
};
