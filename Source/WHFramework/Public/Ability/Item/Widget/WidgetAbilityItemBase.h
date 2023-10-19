// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Widget/Screen/UMG/SubWidgetBase.h"
#include "WidgetAbilityItemBase.generated.h"

/**
 * UI构建项
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetAbilityItemBase : public USubWidgetBase
{
	GENERATED_BODY()
	
public:
	UWidgetAbilityItemBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual int32 GetLimit_Implementation() const override { return 1000; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void OnCreate_Implementation(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnInitialize_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnRefresh_Implementation() override;

	virtual void OnDestroy_Implementation() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FAbilityItem Item;

public:
	FAbilityItem GetItem() const { return Item; }
};
