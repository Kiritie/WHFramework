// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Widget/Screen/UserWidgetBase.h"
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
	TMap<ESlotSplitType, FWidgetInventorySlots> UISlotDatas;

public:
	virtual void OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams) override;
	
	virtual void OnRefresh() override;

public:
	UFUNCTION(BlueprintPure)
	UAbilityInventoryBase* GetInventory() const;

	UFUNCTION(BlueprintPure)
	TArray<UWidgetInventorySlotBase*> GetSplitUISlots(ESlotSplitType InSplitType) const;
};
