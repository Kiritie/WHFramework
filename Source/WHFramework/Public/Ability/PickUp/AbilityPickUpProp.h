// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "AbilityPickUpBase.h"

#include "AbilityPickUpProp.generated.h"

/**
 */
UCLASS()
class WHFRAMEWORK_API AAbilityPickUpProp : public AAbilityPickUpBase
{
	GENERATED_BODY()

public:
	AAbilityPickUpProp();

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

protected:
	virtual void OnPickUp(IAbilityPickerInterface* InPicker) override;

public:
	virtual UMeshComponent* GetMeshComponent() const override;
};
