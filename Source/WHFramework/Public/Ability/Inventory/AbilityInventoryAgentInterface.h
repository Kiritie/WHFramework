// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "UObject/Interface.h"
#include "AbilityInventoryAgentInterface.generated.h"

class UAbilityInventoryBase;

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UAbilityInventoryAgentInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API IAbilityInventoryAgentInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void OnAdditionItem(const FAbilityItem& InItem) = 0;
	
	virtual void OnRemoveItem(const FAbilityItem& InItem) = 0;
	
	virtual void OnActiveItem(const FAbilityItem& InItem, bool bPassive, bool bSuccess) = 0;
		
	virtual void OnDeactiveItem(const FAbilityItem& InItem, bool bPassive) = 0;

	virtual void OnAuxiliaryItem(const FAbilityItem& InItem) = 0;

	virtual void OnDiscardItem(const FAbilityItem& InItem, bool bInPlace) = 0;

	virtual void OnSelectItem(ESlotSplitType InSplitType, const FAbilityItem& InItem) = 0;
	
public:
	virtual UAbilityInventoryBase* GetInventory() const = 0;
};
