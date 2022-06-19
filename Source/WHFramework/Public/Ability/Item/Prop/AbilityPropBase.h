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
	// Sets default values for this actor's properties
	AAbilityPropBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void Initialize(AAbilityCharacterBase* InOwnerCharacter) override;
};
