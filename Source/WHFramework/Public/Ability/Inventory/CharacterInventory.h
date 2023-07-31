// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Inventory/Inventory.h"
#include "CharacterInventory.generated.h"

class UInventorySkillSlot;

/**
 * ��ɫ��Ʒ��
 */
UCLASS()
class WHFRAMEWORK_API UCharacterInventory : public UInventory
{
	GENERATED_BODY()

public:
	UCharacterInventory();

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase = EPhase::Final) override;

	virtual FSaveData* ToData(bool bRefresh) override;

	virtual void UnloadData(EPhase InPhase = EPhase::Final) override;

public:
	virtual FQueryItemInfo QueryItemByRange(EQueryItemType InActionType, FAbilityItem InItem, int32 InStartIndex = 0, int32 InEndIndex = -1) override;

	virtual void DiscardAllItem() override;

	virtual void ClearAllItem() override;

	virtual UInventorySkillSlot* GetSkillSlotByID(const FPrimaryAssetId& InSkillID);
};
