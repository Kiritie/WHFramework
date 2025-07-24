// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Common/Base/WHObject.h"
#include "AbilityInventorySlotBase.generated.h"

class UAbilityInventoryBase;

/**
 * 物品槽
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityInventorySlotBase : public UWHObject
{
	GENERATED_BODY()

public:
	UAbilityInventorySlotBase();

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	//////////////////////////////////////////////////////////////////////////
	/// Initialize
public:
	virtual void OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex);
	
	UFUNCTION(BlueprintCallable)
	virtual void OnItemPreChange(FAbilityItem& InNewItem, bool bBroadcast);
		
	UFUNCTION(BlueprintCallable)
	virtual void OnItemChanged(FAbilityItem& InOldItem, bool bBroadcast);

	//////////////////////////////////////////////////////////////////////////
	/// Checks
public:
	UFUNCTION(BlueprintPure)
	virtual bool ContainsItem(FAbilityItem InItem) const;
	
	UFUNCTION(BlueprintPure)
	virtual bool MatchItem(FAbilityItem InItem, bool bPutIn = false) const;

	UFUNCTION(BlueprintPure)
	virtual bool MatchItemSplit(FAbilityItem InItem, bool bForce = false) const;

	UFUNCTION(BlueprintPure)
	virtual bool MatchItemLimit(FAbilityItem InItem, bool bForce = false) const;

	//////////////////////////////////////////////////////////////////////////
	/// Actions
public:
	UFUNCTION(BlueprintCallable)
	virtual void Reset();

	UFUNCTION(BlueprintCallable)
	virtual void Refresh();

	UFUNCTION(BlueprintCallable)
	virtual void Replace(UAbilityInventorySlotBase* InSlot);

	UFUNCTION(BlueprintCallable)
	virtual void SetItem(FAbilityItem& InItem, bool bRefresh = true);

	UFUNCTION(BlueprintCallable)
	virtual void AddItem(FAbilityItem& InItem);

	UFUNCTION(BlueprintCallable)
	virtual void SubItem(FAbilityItem& InItem);

	virtual void SubItem(int32 InCount = -1);

	UFUNCTION(BlueprintCallable)
	virtual void SplitItem(int32 InCount = -1);
			
	UFUNCTION(BlueprintCallable)
	virtual void MoveItem(int32 InCount = -1);

	UFUNCTION(BlueprintCallable)
	virtual void UseItem(int32 InCount = -1);

	UFUNCTION(BlueprintCallable)
	virtual void DiscardItem(int32 InCount = -1, bool bInPlace = true);

	UFUNCTION(BlueprintCallable)
	virtual bool ActiveItem(bool bPassive = false);
		
	UFUNCTION(BlueprintCallable)
	virtual void DeactiveItem(bool bPassive = false);

	UFUNCTION(BlueprintCallable)
	virtual void ClearItem();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FAbilityItem Item;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	UAbilityInventoryBase* Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	EAbilityItemType LimitType;
		
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	ESlotSplitType SplitType;
			
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	int32 SlotIndex;

public:
	UPROPERTY(BlueprintAssignable)
	FOnInventorySlotRefresh OnSlotRefresh;

	UPROPERTY(BlueprintAssignable)
	FOnInventorySlotActivated OnSlotActivated;

	UPROPERTY(BlueprintAssignable)
	FOnInventorySlottDeactived OnSlotDeactived;

public:
	UFUNCTION(BlueprintPure)
	virtual bool IsEmpty() const;
	
	UFUNCTION(BlueprintPure)
	virtual bool IsEnabled() const;
	
	UFUNCTION(BlueprintPure)
	virtual bool IsSelected() const;

	UFUNCTION(BlueprintPure)
	virtual bool IsMatched() const;

	UFUNCTION(BlueprintPure)
	virtual bool IsLimitMatched(bool bForce = false) const;

	UFUNCTION(BlueprintPure)
	virtual bool IsSplitMatched(bool bForce = false) const;

	int32 GetRemainVolume(FAbilityItem InItem = FAbilityItem::Empty) const;

	int32 GetMaxVolume(FAbilityItem InItem = FAbilityItem::Empty) const;

	int32 GetMaxLevel(FAbilityItem InItem = FAbilityItem::Empty) const;

	UFUNCTION(BlueprintPure)
	FAbilityItem& GetItem() { return Item; }

	UFUNCTION(BlueprintPure)
	UAbilityInventoryBase* GetInventory() const { return Inventory; }
	
	UFUNCTION(BlueprintPure)
	EAbilityItemType GetLimitType() const { return LimitType; }

	UFUNCTION(BlueprintCallable)
	void SetLimitType(EAbilityItemType val) { LimitType = val; }
	
	UFUNCTION(BlueprintPure)
	ESlotSplitType GetSplitType() const { return SplitType; }

	UFUNCTION(BlueprintPure)
	int32 GetSlotIndex() const { return SlotIndex; }

	UFUNCTION(BlueprintPure)
	virtual FAbilityInfo GetAbilityInfo() const;
};
