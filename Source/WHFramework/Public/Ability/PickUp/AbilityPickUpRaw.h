// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "AbilityPickUpBase.h"

#include "AbilityPickUpRaw.generated.h"

/**
 */
UCLASS()
class WHFRAMEWORK_API AAbilityPickUpRaw : public AAbilityPickUpBase
{
	GENERATED_BODY()

public:
	AAbilityPickUpRaw();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

protected:
	virtual void OnPickUp(IAbilityPickerInterface* InPicker) override;

public:
	virtual UMeshComponent* GetMeshComponent() const override;
};
