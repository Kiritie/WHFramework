// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Ability/AbilityModuleTypes.h"
#include "UObject/Interface.h"
#include "InventoryAgentInterface.generated.h"

class UInventory;

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UInventoryAgentInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API IInventoryAgentInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void OnActiveItem(const FAbilityItem& InItem, bool bPassive, bool bSuccess) = 0;
		
	virtual void OnCancelItem(const FAbilityItem& InItem, bool bPassive) = 0;

	virtual void OnAssembleItem(const FAbilityItem& InItem) = 0;

	virtual void OnDischargeItem(const FAbilityItem& InItem) = 0;

	virtual void OnDiscardItem(const FAbilityItem& InItem, bool bInPlace) = 0;

	virtual void OnSelectItem(const FAbilityItem& InItem) = 0;

	virtual void OnAuxiliaryItem(const FAbilityItem& InItem) = 0;
	
public:
	virtual UInventory* GetInventory() const = 0;
};
