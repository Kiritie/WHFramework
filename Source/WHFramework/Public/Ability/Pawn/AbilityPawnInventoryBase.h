// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Inventory/AbilityInventoryBase.h"
#include "AbilityPawnInventoryBase.generated.h"

/**
 * �����Ʒ��
 */
UCLASS()
class WHFRAMEWORK_API UAbilityPawnInventoryBase : public UAbilityInventoryBase
{
	GENERATED_BODY()

public:
	UAbilityPawnInventoryBase();

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;

	virtual void UnloadData(EPhase InPhase) override;

public:
	virtual void DiscardAllItem() override;

	virtual void ClearAllItem() override;

	virtual FItemQueryInfo QueryItemByRange(EItemQueryType InActionType, FAbilityItem InItem, int32 InStartIndex = 0, int32 InEndIndex = -1) override;
};
