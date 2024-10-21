// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Widget/Screen/UserWidgetBase.h"
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

public:
	virtual void OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams) override;
	
	virtual void OnRefresh() override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<ESlotSplitType, FWidgetInventorySlotData> UISlotDatas;

public:
	UFUNCTION(BlueprintPure)
	UAbilityInventoryBase* GetInventory() const;

	UFUNCTION(BlueprintPure)
	TArray<UWidgetAbilityInventorySlotBase*> GetSplitUISlots(ESlotSplitType InSplitType) const;
};
