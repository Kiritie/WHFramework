// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHObject.h"
#include "SaveGame/Base/SaveDataAgentInterface.h"
#include "Ability/AbilityModuleTypes.h"
#include "ReferencePool/ReferencePoolInterface.h"
#include "AbilityInventoryBase.generated.h"

class IAbilityInventoryAgentInterface;
class UAbilityInventoryShortcutSlotBase;
class UAbilityInventoryAuxiliarySlotBase;
class UAbilityInventoryEquipSlotBase;
class UAbilityInventorySkillSlotBase;

/**
 * ��Ʒ��
 */
UCLASS(DefaultToInstanced, Blueprintable)
class WHFRAMEWORK_API UAbilityInventoryBase : public UWHObject, public ISaveDataAgentInterface, public IReferencePoolInterface
{
	GENERATED_BODY()

public:
	UAbilityInventoryBase();
		
	//////////////////////////////////////////////////////////////////////////
	// ReferencePool
public:
	virtual void OnReset_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// SaveData
protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;

	virtual void UnloadData(EPhase InPhase) override;

	//////////////////////////////////////////////////////////////////////////
	/// AbilityInventory
public:
	UFUNCTION(BlueprintPure)
	virtual FItemQueryData QueryItemByRange(EItemQueryType InQueryType, FAbilityItem InItem, int32 InStartIndex = 0, int32 InEndIndex = -1);

	UFUNCTION(BlueprintPure)
	virtual FItemQueryData QueryItemBySplitType(EItemQueryType InQueryType, FAbilityItem InItem, ESlotSplitType InSplitType);
	
	UFUNCTION(BlueprintPure)
	virtual FItemQueryData QueryItemBySplitTypes(EItemQueryType InQueryType, FAbilityItem InItem, const TArray<ESlotSplitType>& InSplitTypes);

public:
	UFUNCTION(BlueprintCallable)
	virtual void AddItemByRange(FAbilityItem& InItem, int32 InStartIndex = 0, int32 InEndIndex = -1, bool bBroadcast = true);

	UFUNCTION(BlueprintCallable)
	virtual void AddItemBySplitType(FAbilityItem& InItem, ESlotSplitType InSplitType, bool bBroadcast = true);
		
	UFUNCTION(BlueprintCallable)
	virtual void AddItemBySplitTypes(FAbilityItem& InItem, const TArray<ESlotSplitType>& InSplitTypes, bool bBroadcast = true);

	UFUNCTION(BlueprintCallable)
	virtual void AddItemByQueryData(FItemQueryData& InQueryData, bool bBroadcast = true);

	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemByRange(FAbilityItem& InItem, int32 InStartIndex = 0, int32 InEndIndex = -1, bool bBroadcast = true);

	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemBySplitType(FAbilityItem& InItem, ESlotSplitType InSplitType, bool bBroadcast = true);
	
	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemBySplitTypes(FAbilityItem& InItem, const TArray<ESlotSplitType>& InSplitTypes, bool bBroadcast = true);

	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemByQueryData(FItemQueryData& InQueryData, bool bBroadcast = true);

	UFUNCTION(BlueprintCallable)
	virtual void MoveItemByRange(UAbilityInventoryBase* InTargetInventory, FAbilityItem& InItem, int32 InSelfStartIndex = 0, int32 InSelfEndIndex = -1, int32 InTargetStartIndex = 0, int32 InTargetEndIndex = -1);

	UFUNCTION(BlueprintCallable)
	virtual void MoveItemBySplitType(UAbilityInventoryBase* InTargetInventory, FAbilityItem& InItem, ESlotSplitType InSelfSlotSplitType, ESlotSplitType InTargetSlotSplitType);

	UFUNCTION(BlueprintCallable)
	virtual void ResetItems();

	UFUNCTION(BlueprintCallable)
	virtual void RefreshItems();

	UFUNCTION(BlueprintCallable)
	virtual void ClearItem(FAbilityItem InItem);

	UFUNCTION(BlueprintCallable)
	virtual void ClearItems();

	UFUNCTION(BlueprintCallable)
	virtual void DiscardItems();

protected:
	UFUNCTION(BlueprintCallable)
	virtual void AddItemBySlots(FAbilityItem& InItem, const TArray<UAbilityInventorySlotBase*>& InSlots, bool bBroadcast);

	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemBySlots(FAbilityItem& InItem, const TArray<UAbilityInventorySlotBase*>& InSlots, bool bBroadcast);

protected:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAbilityInventoryShortcutSlotBase> ShortcutSlotClass;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAbilityInventoryAuxiliarySlotBase> AuxiliarySlotClass;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAbilityInventoryEquipSlotBase> EquipSlotClass;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAbilityInventorySkillSlotBase> SkillSlotClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<ESlotSplitType, FInventorySlots> SplitSlots;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<ESlotSplitType, int32> SelectedIndexs;
				
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UAbilityInventoryBase* ConnectInventory;
	
public:
	UPROPERTY(BlueprintAssignable)
	FOnInventoryRefresh OnRefresh;

	UPROPERTY(BlueprintAssignable)
	FOnInventorySlotSelected OnSlotSelected;

public:
	template<class T>
	T* GetOwnerAgent() const
	{
		return Cast<T>(GetOuter());
	}
	
	UFUNCTION(BlueprintPure)
	TScriptInterface<IAbilityInventoryAgentInterface> GetOwnerAgent() const;
	
	UFUNCTION(BlueprintPure)
	UAbilityInventoryBase* GetConnectInventory() const { return ConnectInventory; }

	UFUNCTION(BlueprintCallable)
	void SetConnectInventory(UAbilityInventoryBase* InInventory);
					
	UFUNCTION(BlueprintPure)
	UAbilityInventorySlotBase* GetSelectedSlot(ESlotSplitType InSplitType) const;

	UFUNCTION(BlueprintCallable)
	void SetSelectedSlot(ESlotSplitType InSplitType, int32 InSlotIndex);
	
	UFUNCTION(BlueprintPure)
	FAbilityItem& GetSelectedItem(ESlotSplitType InSplitType) const;

	UFUNCTION(BlueprintPure)
	bool HasSplitType(ESlotSplitType InSplitType) const;
	
	UFUNCTION(BlueprintPure)
	TMap<ESlotSplitType, FInventorySlots>& GetSplitSlots() { return SplitSlots; }
				
	UFUNCTION(BlueprintPure)
	TArray<UAbilityInventorySlotBase*> GetAllSlots() const;
	
	UFUNCTION(BlueprintPure)
	TArray<FAbilityItem> GetAllItems(bool bMerge = false) const;

	UFUNCTION(BlueprintPure)
	TArray<UAbilityInventorySlotBase*> GetSlotsBySplitType(ESlotSplitType InSplitType) const;

	UFUNCTION(BlueprintPure)
	UAbilityInventorySlotBase* GetSlotBySplitTypeAndIndex(ESlotSplitType InSplitType, int32 InIndex) const;

	UFUNCTION(BlueprintPure)
	UAbilityInventorySlotBase* GetSlotBySplitTypeAndItemID(ESlotSplitType InSplitType, const FPrimaryAssetId& InItemID) const;
};
