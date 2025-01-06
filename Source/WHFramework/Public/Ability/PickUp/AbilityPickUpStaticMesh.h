// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "AbilityPickUpBase.h"

#include "AbilityPickUpStaticMesh.generated.h"

/**
 */
UCLASS()
class WHFRAMEWORK_API AAbilityPickUpStaticMesh : public AAbilityPickUpBase
{
	GENERATED_BODY()

public:
	AAbilityPickUpStaticMesh();

protected:
	virtual void OnPickUp(IAbilityPickerInterface* InPicker) override;

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

public:
	virtual UMeshComponent* GetMeshComponent() const override;
};
