// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "AbilityPickUpBase.h"

#include "AbilityPickUpSkeletalMesh.generated.h"

/**
 */
UCLASS()
class WHFRAMEWORK_API AAbilityPickUpSkeletalMesh : public AAbilityPickUpBase
{
	GENERATED_BODY()

public:
	AAbilityPickUpSkeletalMesh();

protected:
	virtual void OnPickUp(IAbilityPickerInterface* InPicker) override;

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* MeshComponent;

public:
	virtual UMeshComponent* GetMeshComponent() const override;
};
