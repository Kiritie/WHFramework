// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Global/Base/WHObject.h"
#include "InventorySlot.generated.h"

class UInventory;

/**
 * 物品槽
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UInventorySlot : public UWHObject
{
	GENERATED_BODY()

public:
	UInventorySlot();

	//////////////////////////////////////////////////////////////////////////
	/// Properties
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Default")
	FAbilityItem Item;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Default")
	UInventory* Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Default")
	EAbilityItemType LimitType;
		
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Default")
	ESplitSlotType SplitType;
	
public:
	UPROPERTY(BlueprintAssignable)
	FOnInventorySlotRefresh OnInventorySlotRefresh;

	UPROPERTY(BlueprintAssignable)
	FOnInventorySlotActivated OnInventorySlotActivated;

	UPROPERTY(BlueprintAssignable)
	FOnInventorySlotCanceled OnInventorySlotCanceled;

	//////////////////////////////////////////////////////////////////////////
	/// Initialize
public:
	UFUNCTION(BlueprintCallable)
	virtual void OnInitialize(UInventory* InInventory, FAbilityItem InItem, EAbilityItemType InLimitType/* = EAbilityItemType::None*/, ESplitSlotType InSplitType/* = ESplitSlotType::Default*/);
	
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation() override;

	UFUNCTION(BlueprintCallable)
	virtual void OnItemPreChange(FAbilityItem& InNewItem);
		
	UFUNCTION(BlueprintCallable)
	virtual void OnItemChanged(FAbilityItem& InOldItem);

	//////////////////////////////////////////////////////////////////////////
	/// Checks
public:
	UFUNCTION(BlueprintPure)
	virtual bool CheckSlot(FAbilityItem& InItem) const;

	UFUNCTION(BlueprintPure)
	virtual bool CanPutIn(FAbilityItem& InItem) const;
						
	UFUNCTION(BlueprintPure)
	virtual bool IsMatch(FAbilityItem InItem, bool bForce = false) const;

	UFUNCTION(BlueprintPure)
	virtual bool Contains(FAbilityItem& InItem) const;

	//////////////////////////////////////////////////////////////////////////
	/// Actions
public:
	UFUNCTION(BlueprintCallable)
	virtual void Refresh();

	UFUNCTION(BlueprintCallable)
	virtual void Replace(UInventorySlot* InSlot);

	UFUNCTION(BlueprintCallable)
	virtual void SetItem(FAbilityItem& InItem, bool bRefresh = true);

	UFUNCTION(BlueprintCallable)
	virtual void AddItem(FAbilityItem& InItem);

	UFUNCTION(BlueprintCallable)
	virtual void SubItem(FAbilityItem& InItem);
			
	UFUNCTION(BlueprintCallable)
	virtual void SplitItem(int32 InCount = -1);
			
	UFUNCTION(BlueprintCallable)
	virtual void MoveItem(int32 InCount = -1);

	UFUNCTION(BlueprintCallable)
	virtual void UseItem(int32 InCount = -1);

	virtual void AssembleItem();

	virtual void DischargeItem();

	UFUNCTION(BlueprintCallable)
	virtual void DiscardItem(int32 InCount = -1, bool bInPlace = true);

	UFUNCTION(BlueprintCallable)
	virtual bool ActiveItem(bool bPassive = false);
		
	UFUNCTION(BlueprintCallable)
	virtual void CancelItem(bool bPassive = false);

	UFUNCTION(BlueprintCallable)
	virtual void ClearItem();

	//////////////////////////////////////////////////////////////////////////
	/// Getter/Setter
public:
	UFUNCTION(BlueprintPure)
	bool IsEmpty() const;
	
	UFUNCTION(BlueprintPure)
	bool IsSelected() const;

	UFUNCTION(BlueprintPure)
	bool IsMatched(bool bForce = false) const;

	int32 GetSplitIndex(ESplitSlotType InSplitSlotType = ESplitSlotType::None);

	int32 GetRemainVolume(FAbilityItem InItem = FAbilityItem::Empty) const;

	int32 GetMaxVolume(FAbilityItem InItem = FAbilityItem::Empty) const;

	UFUNCTION(BlueprintPure)
	FAbilityItem& GetItem()  { return Item; }

	UFUNCTION(BlueprintPure)
	UInventory* GetInventory() const { return Inventory; }
	
	UFUNCTION(BlueprintPure)
	EAbilityItemType GetLimitType() const { return LimitType; }

	UFUNCTION(BlueprintCallable)
	void SetLimitType(EAbilityItemType val) { LimitType = val; }
	
	UFUNCTION(BlueprintPure)
	ESplitSlotType GetSplitType() const { return SplitType; }

	UFUNCTION(BlueprintPure)
	virtual FAbilityInfo GetAbilityInfo() const;
};
