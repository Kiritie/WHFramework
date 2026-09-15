// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Inventory/AbilityInventoryBase.h"
#include "AbilityVitalityInventoryBase.generated.h"

/**
 * �����Ʒ��
 */
UCLASS()
class WHFRAMEWORK_API UAbilityVitalityInventoryBase : public UAbilityInventoryBase
{
	GENERATED_BODY()

public:
	UAbilityVitalityInventoryBase();

protected:
	virtual void LoadData(const FParameter& InSaveData, EPhase InPhase) override;

	virtual FParameter ToData() override;

	virtual void UnloadData(EPhase InPhase) override;
};
