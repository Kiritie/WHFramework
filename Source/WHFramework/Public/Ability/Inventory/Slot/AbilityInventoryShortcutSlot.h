// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityInventorySlot.h"
#include "AbilityInventoryShortcutSlot.generated.h"

/**
 * ���ܲ�
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityInventoryShortcutSlot : public UAbilityInventorySlot
{
	GENERATED_BODY()

public:
	UAbilityInventoryShortcutSlot();

public:
	virtual void OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex) override;

	virtual void Refresh() override;
};
