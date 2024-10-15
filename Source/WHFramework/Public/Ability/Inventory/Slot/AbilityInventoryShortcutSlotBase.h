// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityInventorySlotBase.h"
#include "AbilityInventoryShortcutSlotBase.generated.h"

/**
 * ���ܲ�
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityInventoryShortcutSlotBase : public UAbilityInventorySlotBase
{
	GENERATED_BODY()

public:
	UAbilityInventoryShortcutSlotBase();

public:
	virtual void OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex) override;

	virtual void Refresh() override;
};
