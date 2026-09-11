// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Widget/Pool/PoolWidgetBase.h"
#include "WidgetAbilityDragItemBase.generated.h"

/**
 * UI拖拽项
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetAbilityDragItemBase : public UPoolWidgetBase
{
	GENERATED_BODY()
	
public:
	UWidgetAbilityDragItemBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:

	virtual void OnSpawn_Implementation(
		const FParameter& InParameter) override;
		
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FAbilityItem Item;

public:
	FAbilityItem GetItem() const { return Item; }
};
