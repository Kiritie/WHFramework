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

	virtual FSaveData* ToData(bool bRefresh) override;

	virtual void UnloadData(EPhase InPhase) override;

public:
	virtual FQueryItemInfo QueryItemByRange(EQueryItemType InActionType, FAbilityItem InItem, int32 InStartIndex = 0, int32 InEndIndex = -1) override;

	virtual void DiscardAllItem() override;

	virtual void ClearAllItem() override;

	virtual UAbilityInventorySkillSlot* GetSkillSlotByID(const FPrimaryAssetId& InSkillID);
};
