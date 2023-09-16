// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Global/Base/WHObject.h"
#include "SaveGame/Base/SaveDataInterface.h"
#include "Ability/AbilityModuleTypes.h"
#include "Inventory.generated.h"

class IInventoryAgentInterface;
class UInventoryShortcutSlot;
class UInventoryAuxiliarySlot;
class UInventoryEquipSlot;
class UInventorySkillSlot;

/**
 * ��Ʒ��
 */
UCLASS(DefaultToInstanced, Blueprintable)
class WHFRAMEWORK_API UInventory : public UWHObject, public ISaveDataInterface
{
	GENERATED_BODY()

public:
	UInventory();
	
	//////////////////////////////////////////////////////////////////////////
	/// Properties
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<ESplitSlotType, FSplitSlotInfo> SplitInfos;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<UInventorySlot*> Slots;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UInventory* ConnectInventory;
				
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UInventorySlot* SelectedSlot;
				
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UInventoryShortcutSlot> ShortcutSlotClass;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UInventoryAuxiliarySlot> AuxiliarySlotClass;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UInventoryEquipSlot> EquipSlotClass;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UInventorySkillSlot> SkillSlotClass;
	
public:
	UPROPERTY(BlueprintAssignable)
	FOnInventoryRefresh OnRefresh;

	UPROPERTY(BlueprintAssignable)
	FOnInventorySlotSelected OnSlotSelected;

	//////////////////////////////////////////////////////////////////////////
	/// Data
protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData(bool bRefresh) override;

	virtual void UnloadData(EPhase InPhase) override;

	//////////////////////////////////////////////////////////////////////////
	/// Actions
public:
	UFUNCTION(BlueprintPure)
	virtual FQueryItemInfo QueryItemByRange(EQueryItemType InQueryType, FAbilityItem InItem, int32 InStartIndex = 0, int32 InEndIndex = -1);

	UFUNCTION(BlueprintPure)
	virtual FQueryItemInfo QueryItemBySplitType(EQueryItemType InQueryType, FAbilityItem InItem, ESplitSlotType InSplitSlotType);
	
	UFUNCTION(BlueprintPure)
	virtual FQueryItemInfo QueryItemBySplitTypes(EQueryItemType InQueryType, FAbilityItem InItem, const TArray<ESplitSlotType>& InSplitSlotTypes);

public:
	UFUNCTION(BlueprintCallable)
	virtual void AddItemBySlots(FAbilityItem& InItem, const TArray<UInventorySlot*>& InSlots);

	UFUNCTION(BlueprintCallable)
	virtual void AddItemByRange(FAbilityItem& InItem, int32 InStartIndex = 0, int32 InEndIndex = -1);

	UFUNCTION(BlueprintCallable)
	virtual void AddItemBySplitType(FAbilityItem& InItem, ESplitSlotType InSplitSlotType);
		
	UFUNCTION(BlueprintCallable)
	virtual void AddItemBySplitTypes(FAbilityItem& InItem, const TArray<ESplitSlotType>& InSplitSlotTypes);

	UFUNCTION(BlueprintCallable)
	virtual void AddItemByQueryInfo(FQueryItemInfo& InQueryInfo);

	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemBySlots(FAbilityItem& InItem, const TArray<UInventorySlot*>& InSlots);

	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemByRange(FAbilityItem& InItem, int32 InStartIndex = 0, int32 InEndIndex = -1);

	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemBySplitType(FAbilityItem& InItem, ESplitSlotType InSplitSlotType);
	
	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemBySplitTypes(FAbilityItem& InItem, const TArray<ESplitSlotType>& InSplitSlotTypes);

	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemByQueryInfo(FQueryItemInfo& InQueryInfo);

	UFUNCTION(BlueprintCallable)
	virtual void MoveItemByRange(UInventory* InTargetInventory, FAbilityItem& InItem, int32 InSelfStartIndex = 0, int32 InSelfEndIndex = -1, int32 InTargetStartIndex = 0, int32 InTargetEndIndex = -1);

	UFUNCTION(BlueprintCallable)
	virtual void MoveItemBySplitType(UInventory* InTargetInventory, FAbilityItem& InItem, ESplitSlotType InSelfSplitSlotType, ESplitSlotType InTargetSplitSlotType);

	UFUNCTION(BlueprintCallable)
	virtual void ClearItem(FAbilityItem& InItem);

	UFUNCTION(BlueprintCallable)
	virtual void ClearAllItem();

	UFUNCTION(BlueprintCallable)
	virtual void DiscardAllItem();

	//////////////////////////////////////////////////////////////////////////
	/// Setter/Getter
public:
	template<class T>
	T* GetOwnerAgent() const
	{
		return Cast<T>(GetOuter());
	}
	
	UFUNCTION(BlueprintPure)
	TScriptInterface<IInventoryAgentInterface> GetOwnerAgent() const;
	
	UFUNCTION(BlueprintPure)
	UInventory* GetConnectInventory() const { return ConnectInventory; }

	UFUNCTION(BlueprintCallable)
	void SetConnectInventory(UInventory* InInventory);

	UFUNCTION(BlueprintPure)
	int32 GetSlotsNum() const;

	UFUNCTION(BlueprintPure)
	TArray<UInventorySlot*> GetSlots() const { return Slots; }
					
	UFUNCTION(BlueprintPure)
	UInventorySlot* GetSelectedSlot() const { return SelectedSlot; }

	UFUNCTION(BlueprintCallable)
	void SetSelectedSlot(UInventorySlot* InSelectedSlot);
	
	UFUNCTION(BlueprintPure)
	FAbilityItem& GetSelectedItem() const;

	UFUNCTION(BlueprintPure)
	bool HasSplitSlotInfo(ESplitSlotType InSplitSlotType) const;

	UFUNCTION(BlueprintPure)
	FSplitSlotInfo GetSplitSlotInfo(ESplitSlotType InSplitSlotType) const;
	
	UFUNCTION(BlueprintPure)
	TMap<ESplitSlotType, FSplitSlotInfo> GetSplitSlotInfos() const { return SplitInfos; }

	template<typename T>
	TArray<T*> GetSplitSlots(ESplitSlotType InSplitSlotType)
	{
		TArray<T*> SplitSlots;
		for (auto Iter : GetSplitSlots(InSplitSlotType))
		{
			SplitSlots.Add(Cast<T>(Iter));
		}
		return SplitSlots;
	}
		
	UFUNCTION(BlueprintPure)
	TArray<UInventorySlot*> GetSplitSlots(ESplitSlotType InSplitSlotType);

	UFUNCTION(BlueprintPure)
	TMap<ESplitSlotType, FSplitSlotData> GetSplitSlotDatas();
};
