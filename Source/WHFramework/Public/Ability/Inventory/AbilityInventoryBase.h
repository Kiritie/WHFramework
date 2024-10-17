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
	// Reference
public:
	virtual void OnReset_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// Properties
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<ESlotSplitType, FInventorySlots> SplitSlots;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<ESlotSplitType, UAbilityInventorySlotBase*> SelectedSlots;
				
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UAbilityInventoryBase* ConnectInventory;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAbilityInventoryShortcutSlotBase> ShortcutSlotClass;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAbilityInventoryAuxiliarySlotBase> AuxiliarySlotClass;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAbilityInventoryEquipSlotBase> EquipSlotClass;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAbilityInventorySkillSlotBase> SkillSlotClass;
	
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
	virtual void AddItemByRange(FAbilityItem& InItem, int32 InStartIndex = 0, int32 InEndIndex = -1, bool bBroadcast = true);

	UFUNCTION(BlueprintCallable)
	virtual void AddItemBySplitType(FAbilityItem& InItem, ESlotSplitType InSplitType, bool bBroadcast = true);
		
	UFUNCTION(BlueprintCallable)
	virtual void AddItemBySplitTypes(FAbilityItem& InItem, const TArray<ESlotSplitType>& InSplitTypes, bool bBroadcast = true);

	UFUNCTION(BlueprintCallable)
	virtual void AddItemByQueryInfo(FItemQueryInfo& InQueryInfo, bool bBroadcast = true);

	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemByRange(FAbilityItem& InItem, int32 InStartIndex = 0, int32 InEndIndex = -1, bool bBroadcast = true);

	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemBySplitType(FAbilityItem& InItem, ESlotSplitType InSplitType, bool bBroadcast = true);
	
	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemBySplitTypes(FAbilityItem& InItem, const TArray<ESlotSplitType>& InSplitTypes, bool bBroadcast = true);

	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemByQueryInfo(FItemQueryInfo& InQueryInfo, bool bBroadcast = true);

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

protected:
	UFUNCTION(BlueprintCallable)
	virtual void AddItemBySlots(FAbilityItem& InItem, const TArray<UAbilityInventorySlotBase*>& InSlots, bool bBroadcast);

	UFUNCTION(BlueprintCallable)
	virtual void RemoveItemBySlots(FAbilityItem& InItem, const TArray<UAbilityInventorySlotBase*>& InSlots, bool bBroadcast);

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
	UAbilityInventorySlotBase* GetSelectedSlot(ESlotSplitType InSplitType) const;

	UFUNCTION(BlueprintCallable)
	void SetSelectedSlot(ESlotSplitType InSplitType, UAbilityInventorySlotBase* InSelectedSlot);
	
	UFUNCTION(BlueprintPure)
	FAbilityItem& GetSelectedItem(ESlotSplitType InSplitType) const;

	UFUNCTION(BlueprintPure)
	bool HasSplitType(ESlotSplitType InSplitType) const;
	
	UFUNCTION(BlueprintPure)
	TMap<ESlotSplitType, FInventorySlots>& GetSplitSlots() { return SplitSlots; }
				
	UFUNCTION(BlueprintPure)
	TArray<UAbilityInventorySlotBase*> GetSlots();

	UFUNCTION(BlueprintPure)
	TArray<UAbilityInventorySlotBase*> GetSlotsBySplitType(ESlotSplitType InSplitType);

	UFUNCTION(BlueprintPure)
	UAbilityInventorySlotBase* GetSlotBySplitTypeAndIndex(ESlotSplitType InSplitType, int32 InIndex);

	UFUNCTION(BlueprintPure)
	UAbilityInventorySlotBase* GetSlotBySplitTypeAndItemID(ESlotSplitType InSplitType, const FPrimaryAssetId& InItemID);
};
