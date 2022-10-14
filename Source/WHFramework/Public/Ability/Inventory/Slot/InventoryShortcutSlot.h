// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "InventorySlot.h"
#include "InventoryShortcutSlot.generated.h"

/**
 * ���ܲ�
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UInventoryShortcutSlot : public UInventorySlot
{
	GENERATED_BODY()

public:
	UInventoryShortcutSlot();

public:
	virtual void OnInitialize(UInventory* InInventory, FAbilityItem InItem, EAbilityItemType InLimitType /* = EAbilityItemType::None */, ESplitSlotType InSplitType /*= ESplitSlotType::Default*/) override;

	virtual void Refresh() override;
};
