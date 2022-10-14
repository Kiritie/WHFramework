// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Ability/AbilityModuleTypes.h"
#include "WidgetInventorySlotBase.generated.h"

class UInventorySlot;
class UInventory;

/**
 * UI物品槽
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetInventorySlotBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UWidgetInventorySlotBase(const FObjectInitializer& ObjectInitializer);

public:
	UFUNCTION()
	virtual void OnInitialize(UInventorySlot* InOwnerSlot);

protected:
	UFUNCTION()
	virtual void OnRefresh();

	UFUNCTION()
	virtual void OnActivated();
	
	UFUNCTION()
	virtual void OnCanceled();
		
protected:
	UFUNCTION()
	virtual void StartCooldown();
			
	UFUNCTION()
	virtual void StopCooldown();

	UFUNCTION()
	virtual void OnCooldown();

public:
	UFUNCTION(BlueprintCallable)
	virtual void SplitItem(int InCount = -1);
	
	UFUNCTION(BlueprintCallable)
	virtual void MoveItem(int InCount = -1);

	UFUNCTION(BlueprintCallable)
	virtual void UseItem(int InCount = -1);

	UFUNCTION(BlueprintCallable)
	virtual void DiscardItem(int InCount = -1);
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UInventorySlot* OwnerSlot;

	FTimerHandle CooldownTimerHandle;

public:
	UFUNCTION(BlueprintPure)
	bool IsEmpty() const;

	UFUNCTION(BlueprintPure)
	bool IsCooldowning() const;

	UFUNCTION(BlueprintPure)
	FAbilityItem& GetItem() const;

	UFUNCTION(BlueprintPure)
	UInventorySlot* GetOwnerSlot() const { return OwnerSlot; }

	UFUNCTION(BlueprintPure)
	UInventory* GetInventory() const;
};
