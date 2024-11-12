// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Item/AbilityItemBase.h"
#include "AbilityEquipBase.generated.h"

class UMeshComponent;

/**
 * 装备基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityEquipBase : public AAbilityItemBase
{
	GENERATED_BODY()
	
public:	
	AAbilityEquipBase();

public:
	UFUNCTION(BlueprintNativeEvent)
	void OnAssemble();
	
	UFUNCTION(BlueprintNativeEvent)
	void OnDisassemble();

public:
	UFUNCTION(BlueprintPure)
	virtual UMeshComponent* GetMeshComponent() const;
};
