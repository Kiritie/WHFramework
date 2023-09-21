// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Widget/Screen/UMG/SubWidgetBase.h"
#include "WidgetAbilityInventorySlotBase.generated.h"

class UAbilityInventorySlot;
class UAbilityInventoryBase;

/**
 * UI物品槽
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetAbilityInventorySlotBase : public USubWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetAbilityInventorySlotBase(const FObjectInitializer& ObjectInitializer);

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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText KeyCode;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<class UWidgetAbilityInventoryItemBase> DragVisualClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UAbilityInventorySlot* OwnerSlot;

	FTimerHandle CooldownTimerHandle;

public:
	UFUNCTION(BlueprintPure)
	bool IsEmpty() const;

	UFUNCTION(BlueprintPure)
	bool IsCooldowning() const;
	FText GetKeyCode() const { return KeyCode; }
	
	void SetKeyCode(const FText& InKeyCode) { this->KeyCode = InKeyCode; }


	UFUNCTION(BlueprintPure)
	FAbilityItem& GetItem() const;

	UFUNCTION(BlueprintPure)
	UAbilityInventorySlot* GetOwnerSlot() const { return OwnerSlot; }

	UFUNCTION(BlueprintPure)
	UAbilityInventoryBase* GetInventory() const;
};
