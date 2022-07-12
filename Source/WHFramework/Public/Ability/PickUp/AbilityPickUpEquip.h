// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "AbilityPickUpBase.h"

#include "AbilityPickUpEquip.generated.h"

/**
 */
UCLASS()
class WHFRAMEWORK_API AAbilityPickUpEquip : public AAbilityPickUpBase
{
	GENERATED_BODY()

public:
	AAbilityPickUpEquip();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

public:
	virtual void Initialize(FAbilityItem InItem) override;

protected:
	virtual void OnPickUp(IAbilityPickerInterface* InPicker) override;

public:
	virtual UMeshComponent* GetMeshComponent() const override;
};
