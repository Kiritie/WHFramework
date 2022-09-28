// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Item/AbilityItemBase.h"
#include "AbilityPropBase.generated.h"

/**
 * 技能基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityPropBase : public AAbilityItemBase
{
	GENERATED_BODY()
	
public:	
	AAbilityPropBase();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

public:
	virtual void Initialize_Implementation(AAbilityCharacterBase* InOwnerCharacter, const FAbilityItem& InItem = FAbilityItem::Empty) override;
};
