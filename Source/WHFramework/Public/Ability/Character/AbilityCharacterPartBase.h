// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Character/Base/CharacterPartBase.h"

#include "AbilityCharacterPartBase.generated.h"

/**
 * 角色部位组件
 */
UCLASS()
class WHFRAMEWORK_API UAbilityCharacterPartBase : public UCharacterPartBase
{
	GENERATED_BODY()
	
public:
	UAbilityCharacterPartBase(const FObjectInitializer& ObjectInitializer);
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	virtual void UpdateVoxelOverlap() override;
};
