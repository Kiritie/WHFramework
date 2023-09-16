// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelInteractAuxiliary.h"
#include "Ability/Inventory/InventoryAgentInterface.h"
#include "VoxelContainerAuxiliary.generated.h"

class UVoxel;

/**
 */
UCLASS()
class WHFRAMEWORK_API AVoxelContainerAuxiliary : public AVoxelInteractAuxiliary, public IInventoryAgentInterface
{
	GENERATED_BODY()
	
public:
	AVoxelContainerAuxiliary();

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData(bool bRefresh) override;

public:
	virtual void OnActiveItem(const FAbilityItem& InItem, bool bPassive, bool bSuccess) override;
		
	virtual void OnCancelItem(const FAbilityItem& InItem, bool bPassive) override;

	virtual void OnAssembleItem(const FAbilityItem& InItem) override;

	virtual void OnDischargeItem(const FAbilityItem& InItem) override;

	virtual void OnDiscardItem(const FAbilityItem& InItem, bool bInPlace) override;

	virtual void OnSelectItem(const FAbilityItem& InItem) override;

	virtual void OnAuxiliaryItem(const FAbilityItem& InItem) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInventory* Inventory;

public:
	virtual UInventory* GetInventory() const override { return Inventory; }
};
