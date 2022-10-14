// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Item/AbilityItemBase.h"
#include "AbilityEquipBase.generated.h"

class AAbilityCharacterBase;
class UStaticMeshComponent;

/**
 * 装备基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityEquipBase : public AAbilityItemBase
{
	GENERATED_BODY()
	
public:	
	AAbilityEquipBase();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

public:
	virtual void Initialize_Implementation(AAbilityCharacterBase* InOwnerCharacter, const FAbilityItem& InItem = FAbilityItem::Empty) override;

public:
	UFUNCTION(BlueprintNativeEvent)
	void OnAssemble();
	
	UFUNCTION(BlueprintNativeEvent)
	void OnDischarge();

public:
	UFUNCTION(BlueprintPure)
	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }
};
