// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelSwitchAuxiliary.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "VoxelContainerAuxiliary.generated.h"

class UVoxel;

/**
 */
UCLASS()
class WHFRAMEWORK_API AVoxelContainerAuxiliary : public AVoxelSwitchAuxiliary, public IAbilityInventoryAgentInterface
{
	GENERATED_BODY()
	
public:
	AVoxelContainerAuxiliary();

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;

public:
	virtual void OnAdditionItem(const FAbilityItem& InItem) override;

	virtual void OnRemoveItem(const FAbilityItem& InItem) override;

	virtual void OnPreChangeItem(const FAbilityItem& InOldItem) override;

	virtual void OnChangeItem(const FAbilityItem& InNewItem) override;
	
	virtual void OnActiveItem(const FAbilityItem& InItem, bool bPassive, bool bSuccess) override;
		
	virtual void OnDeactiveItem(const FAbilityItem& InItem, bool bPassive) override;

	virtual void OnDiscardItem(const FAbilityItem& InItem, bool bInPlace) override;

	virtual void OnSelectItem(const FAbilityItem& InItem) override;

	virtual void OnAuxiliaryItem(const FAbilityItem& InItem) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAbilityInventoryBase* Inventory;

public:
	virtual UAbilityInventoryBase* GetInventory() const override { return Inventory; }
};
