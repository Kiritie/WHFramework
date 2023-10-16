// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Inventory/AbilityInventoryBase.h"
#include "AbilityCharacterInventoryBase.generated.h"

class UAbilityInventorySkillSlot;

/**
 * ��ɫ��Ʒ��
 */
UCLASS()
class WHFRAMEWORK_API UAbilityCharacterInventoryBase : public UAbilityInventoryBase
{
	GENERATED_BODY()

public:
	UAbilityCharacterInventoryBase();

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;

	virtual void UnloadData(EPhase InPhase) override;

public:
	virtual FItemQueryInfo QueryItemByRange(EItemQueryType InActionType, FAbilityItem InItem, int32 InStartIndex = 0, int32 InEndIndex = -1) override;

	virtual void DiscardAllItem() override;

	virtual void ClearAllItem() override;
};
