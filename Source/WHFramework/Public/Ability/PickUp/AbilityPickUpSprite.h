// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "AbilityPickUpBase.h"

#include "AbilityPickUpSprite.generated.h"

class UWorldWidgetComponent;
/**
 */
UCLASS()
class WHFRAMEWORK_API AAbilityPickUpSprite : public AAbilityPickUpBase
{
	GENERATED_BODY()

public:
	AAbilityPickUpSprite();

public:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

protected:
	virtual void OnPickUp(IAbilityPickerInterface* InPicker) override;

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UWorldWidgetComponent* MeshComponent;

public:
	virtual UMeshComponent* GetMeshComponent() const override;
};
