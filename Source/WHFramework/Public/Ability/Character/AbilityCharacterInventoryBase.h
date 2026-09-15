// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Inventory/AbilityInventoryBase.h"
#include "AbilityCharacterInventoryBase.generated.h"

class UAbilityInventorySkillSlotBase;

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
	virtual void LoadData(const FParameter& InSaveData, EPhase InPhase) override;

	virtual FParameter ToData() override;

	virtual void UnloadData(EPhase InPhase) override;
};
