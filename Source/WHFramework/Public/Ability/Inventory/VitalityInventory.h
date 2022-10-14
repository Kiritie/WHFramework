// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Inventory/Inventory.h"
#include "VitalityInventory.generated.h"

/**
 * �����Ʒ��
 */
UCLASS()
class WHFRAMEWORK_API UVitalityInventory : public UInventory
{
	GENERATED_BODY()

public:
	UVitalityInventory();

protected:
	virtual void LoadData(FSaveData* InSaveData, bool bForceMode) override;

	virtual FSaveData* ToData() override;

	virtual void UnloadData(bool bForceMode) override;

public:
	virtual void DiscardAllItem() override;

	virtual void ClearAllItem() override;

	virtual FQueryItemInfo QueryItemByRange(EQueryItemType InActionType, FAbilityItem InItem, int32 InStartIndex = 0, int32 InEndIndex = -1) override;
};
