// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Widget/Screen/UMG/UserWidgetBase.h"
#include "Ability/AbilityModuleTypes.h"

#include "WidgetAbilityInventoryBase.generated.h"

class UWidgetInventoryBaseSlotBase;

/**
 * ��Ʒ������
 */
UCLASS()
class WHFRAMEWORK_API UWidgetAbilityInventoryBase : public UUserWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetAbilityInventoryBase(const FObjectInitializer& ObjectInitializer);

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
	TArray<UWidgetAbilityInventorySlotBase*> GetSplitUISlots(ESlotSplitType InSplitType) const;
};
