// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Widget/Screen/UMG/UserWidgetBase.h"
#include "Ability/AbilityModuleTypes.h"

#include "WidgetInventoryBase.generated.h"

class UWidgetInventoryBaseSlotBase;

/**
 * ��Ʒ������
 */
UCLASS()
class WHFRAMEWORK_API UWidgetInventoryBase : public UUserWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetInventoryBase(const FObjectInitializer& ObjectInitializer);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TMap<ESplitSlotType, FWidgetSplitSlotData> UISlotDatas;

public:
	virtual void OnInitialize_Implementation(UObject* InOwner) override;
	
	virtual void OnRefresh_Implementation() override;

public:
	UFUNCTION(BlueprintPure)
	UInventory* GetInventory() const;

	UFUNCTION(BlueprintPure)
	TArray<UWidgetInventorySlotBase*> GetSplitUISlots(ESplitSlotType InSplitSlotType) const;
};
