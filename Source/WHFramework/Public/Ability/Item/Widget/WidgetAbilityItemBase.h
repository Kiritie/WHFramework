// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Widget/Sub/SubButtonWidgetBase.h"
#include "WidgetAbilityItemBase.generated.h"

/**
 * UI构建项
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetAbilityItemBase : public USubButtonWidgetBase
{
	GENERATED_BODY()
	
public:
	UWidgetAbilityItemBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:

	virtual void OnSpawn_Implementation(
		const FParameter& InParameter) override;
		
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;

public:
	virtual void OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnInitialize(const TArray<FParameter>& InParams) override;

	virtual void OnRefresh() override;

	virtual void OnDestroy(bool bRecovery) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FAbilityItem Item; 

public:
	FAbilityItem GetItem() const { return Item; }
};
