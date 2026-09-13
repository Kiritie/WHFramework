// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "GameplayTagContainer.h"
#include "Widget/Sub/SubButtonWidgetBase.h"
#include "WidgetAbilityInventorySlotBase.generated.h"

class UAbilityInventorySlotBase;
class UAbilityInventoryBase;

/**
 * UI物品槽
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetAbilityInventorySlotBase : public USubButtonWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetAbilityInventorySlotBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:

	virtual void OnSpawn_Implementation(const FParameter& InParam) override;
	
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;

public:
	virtual void OnCreate(const FParameter& InParam) override;

	virtual void OnInitialize(const FParameter& InParam) override;

	virtual void OnRefresh() override;

	virtual void OnDestroy(EObjectDespawnMode InMode) override;

public:
	virtual bool NativeOnDrop( const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation ) override;

	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	virtual FReply NativeOnMouseMove( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent ) override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

public:
	UFUNCTION(BlueprintNativeEvent)
	void OnActivated();
	
	UFUNCTION(BlueprintNativeEvent)
	void OnDeactivated();
		
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ExposeOnSpawn = true, Categories = "Style.Button"), Category = "Style")
	FGameplayTag MatchStyleTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ExposeOnSpawn = true, Categories = "Style.Button"), Category = "Style")
	FGameplayTag MismatchStyleTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UAbilityInventorySlotBase* OwnerSlot;

	FGameplayTag DefaultStyleTag;

	FTimerHandle CooldownTimerHandle;

public:
	UFUNCTION(BlueprintPure)
	bool IsEmpty() const;

	UFUNCTION(BlueprintPure)
	bool IsCooldowning() const;

	UFUNCTION(BlueprintPure)
	FAbilityItem& GetItem() const;

	UFUNCTION(BlueprintPure)
	TArray<FAbilityItem>& GetMatchItems() const;

	UFUNCTION(BlueprintPure)
	UAbilityInventorySlotBase* GetOwnerSlot() const { return OwnerSlot; }

	UFUNCTION(BlueprintPure)
	UAbilityInventoryBase* GetInventory() const;
};
