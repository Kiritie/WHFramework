// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Item/AbilityItemBase.h"
#include "AbilityRawBase.generated.h"

/**
 * 技能基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityRawBase : public AAbilityItemBase
{
	GENERATED_BODY()
	
public:	
	AAbilityRawBase();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

public:
	virtual void Initialize_Implementation(AActor* InOwnerActor, const FAbilityItem& InItem = FAbilityItem::Empty) override;
};
