// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Blueprint/UserWidget.h"
#include "WidgetAbilityDragItemBase.generated.h"

/**
 * UI拖拽项
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetAbilityDragItemBase : public UUserWidget, public IObjectPoolInterface
{
	GENERATED_BODY()
	
public:
	UWidgetAbilityDragItemBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FAbilityItem Item;

public:
	FAbilityItem GetItem() const { return Item; }
};
