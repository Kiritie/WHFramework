// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "InventorySlot.h"
#include "InventoryAuxiliarySlot.generated.h"

/**
 * ���ܲ�
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UInventoryAuxiliarySlot : public UInventorySlot
{
	GENERATED_BODY()

public:
	UInventoryAuxiliarySlot();

public:
	virtual void OnInitialize(UInventory* InInventory, FAbilityItem InItem, EAbilityItemType InLimitType /* = EAbilityItemType::None */, ESplitSlotType InSplitType /*= ESplitSlotType::Default*/) override;

	virtual void OnItemChanged(FAbilityItem& InOldItem) override;
};
