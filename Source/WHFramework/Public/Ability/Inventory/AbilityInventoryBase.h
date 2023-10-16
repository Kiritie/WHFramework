// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHObject.h"
#include "SaveGame/Base/SaveDataInterface.h"
#include "Ability/AbilityModuleTypes.h"
#include "AbilityInventoryBase.generated.h"

class IAbilityInventoryAgentInterface;
class UAbilityInventoryShortcutSlot;
class UAbilityInventoryAuxiliarySlot;
class UAbilityInventoryEquipSlot;
class UAbilityInventorySkillSlot;

/**
 * ��Ʒ��
 */
UCLASS(DefaultToInstanced, Blueprintable)
class WHFRAMEWORK_API UAbilityInventoryBase : public UWHObject, public ISaveDataInterface
{
	GENERATED_BODY()

public:
	UAbilityInventoryBase();
	
	//////////////////////////////////////////////////////////////////////////
	/// Properties
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<ESlotSplitType, FInventorySlots> SplitSlots;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UAbilityInventoryBase* ConnectInventory;
				
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UAbilityInventorySlot* SelectedSlot;
				
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAbilityInventoryShortcutSlot> ShortcutSlotClass;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAbilityInventoryAuxiliarySlot> AuxiliarySlotClass;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAbilityInventoryEquipSlot> EquipSlotClass;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAbilityInventorySkillSlot> SkillSlotClass;
	
public:
	UPROPERTY(BlueprintAssignable)
	FOnInventoryRefresh OnRefresh;

	UPROPERTY(BlueprintAssignable)
	FOnInventorySlotSelected OnSlotSelected;

	//////////////////////////////////////////////////////////////////////////
	/// Data
protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;

	virtual void UnloadData(EPhase InPhase) override;

	//////////////////////////////////////////////////////////////////////////
	/// Actions
public:
	UFUNCTION(BlueprintPure)
	virtual FItemQueryInfo QueryItemByRange(EItemQueryType InQueryType, FAbilityItem InItem, int32 InStartIndex = 0, int32 InEndIndex = -1);

	UFUNCTION(BlueprintPure)
	virtual FItemQueryInfo QueryItemBySplitType(EItemQueryType InQueryType, FAbilityItem InItem, ESlotSplitType InSplitType);
	
	UFUNCTION(BlueprintPure)
	virtual FItemQueryInfo QueryItemBySplitTypes(EItemQueryType InQueryType, FAbilityItem InItem, const TArray<ESlotSplitType>& InSplitTypes);

public:
	UFUNCTION(BlueprintCallable)
	virtual void AddItemBySlots(FAbilityItem& InItem, const TArray<UAbilityInventorySlot*>& InSlots);

	UFUNCTION(BlueprintCallable)
	virtual void AddItemByRange(FAbilityItem& InItem, int32 InStartIndex = 0, int32 InEndIndex = -1);

	UFUNCTION(BlueprintCallable)
	virtual void AddItemBySplitType(FAbilityItem& InItem, ESlotSplitType InSplitType);
		
	UFUNCTION(BlueprintCallable)
	virtual void AddItemBySplitTypes(FAbilityItem& InItem, const TArray<ESlotSplitType>& InSplitTypes);

	UFUNCTION(BlueprintCallable)
	virtual void AddItemByQueryInfo(FItemQueryInfo& InQueryInfo);

	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemBySlots(FAbilityItem& InItem, const TArray<UAbilityInventorySlot*>& InSlots);

	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemByRange(FAbilityItem& InItem, int32 InStartIndex = 0, int32 InEndIndex = -1);

	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemBySplitType(FAbilityItem& InItem, ESlotSplitType InSplitType);
	
	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemBySplitTypes(FAbilityItem& InItem, const TArray<ESlotSplitType>& InSplitTypes);

	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemByQueryInfo(FItemQueryInfo& InQueryInfo);

	UFUNCTION(BlueprintCallable)
	virtual void MoveItemByRange(UAbilityInventoryBase* InTargetInventory, FAbilityItem& InItem, int32 InSelfStartIndex = 0, int32 InSelfEndIndex = -1, int32 InTargetStartIndex = 0, int32 InTargetEndIndex = -1);

	UFUNCTION(BlueprintCallable)
	virtual void MoveItemBySplitType(UAbilityInventoryBase* InTargetInventory, FAbilityItem& InItem, ESlotSplitType InSelfSlotSplitType, ESlotSplitType InTargetSlotSplitType);

	UFUNCTION(BlueprintCallable)
	virtual void ClearItem(FAbilityItem InItem);

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
	TScriptInterface<IAbilityInventoryAgentInterface> GetOwnerAgent() const;
	
	UFUNCTION(BlueprintPure)
	UAbilityInventoryBase* GetConnectInventory() const { return ConnectInventory; }

	UFUNCTION(BlueprintCallable)
	void SetConnectInventory(UAbilityInventoryBase* InInventory);
					
	UFUNCTION(BlueprintPure)
	UAbilityInventorySlot* GetSelectedSlot() const { return SelectedSlot; }

	UFUNCTION(BlueprintCallable)
	void SetSelectedSlot(UAbilityInventorySlot* InSelectedSlot);
	
	UFUNCTION(BlueprintPure)
	FAbilityItem& GetSelectedItem() const;

	UFUNCTION(BlueprintPure)
	bool HasSplitType(ESlotSplitType InSplitType) const;
	
	UFUNCTION(BlueprintPure)
	TMap<ESlotSplitType, FInventorySlots>& GetSplitSlots() { return SplitSlots; }
				
	UFUNCTION(BlueprintPure)
	TArray<UAbilityInventorySlot*> GetSlots();

	UFUNCTION(BlueprintPure)
	TArray<UAbilityInventorySlot*> GetSlotsBySplitType(ESlotSplitType InSplitType);

	UFUNCTION(BlueprintPure)
	UAbilityInventorySlot* GetSlotBySplitTypeAndIndex(ESlotSplitType InSplitType, int32 InIndex);

	UFUNCTION(BlueprintPure)
	UAbilityInventorySlot* GetSlotBySplitTypeAndItemID(ESlotSplitType InSplitType, const FPrimaryAssetId& InItemID);
};
