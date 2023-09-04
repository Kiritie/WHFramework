// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Widget/Screen/UMG/SubWidgetBase.h"
#include "WidgetInventorySlotBase.generated.h"

class UInventorySlot;
class UInventory;

/**
 * UI物品槽
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetInventorySlotBase : public USubWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetInventorySlotBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual int32 GetLimit_Implementation() const override { return 1000; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void OnCreate_Implementation(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnInitialize_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnRefresh_Implementation() override;

	virtual void OnDestroy_Implementation() override;

public:
	UFUNCTION(BlueprintNativeEvent)
	void OnActivated();
	
	UFUNCTION(BlueprintNativeEvent)
	void OnCanceled();
		
protected:
	UFUNCTION(BlueprintNativeEvent)
	void StartCooldown();
			
	UFUNCTION(BlueprintNativeEvent)
	void StopCooldown();

	UFUNCTION(BlueprintNativeEvent)
	void OnCooldown();

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SplitItem(int InCount = -1);
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void MoveItem(int InCount = -1);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void UseItem(int InCount = -1);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void DiscardItem(int InCount = -1);
	
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
