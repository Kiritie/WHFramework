// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "AbilityPickUpBase.h"

#include "AbilityPickUpSkill.generated.h"

class UPaperSpriteComponent;
/**
 */
UCLASS()
class WHFRAMEWORK_API AAbilityPickUpSkill : public AAbilityPickUpBase
{
	GENERATED_BODY()

public:
	AAbilityPickUpSkill();

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UPaperSpriteComponent* MeshComponent;

protected:
	virtual void OnPickUp(IAbilityPickerInterface* InPicker) override;

public:
	virtual UMeshComponent* GetMeshComponent() const override;
};
