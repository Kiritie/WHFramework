// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "AbilityPickUpBase.h"

#include "AbilityPickUpVoxel.generated.h"

/**
 * 可拾取体素
 */
UCLASS()
class WHFRAMEWORK_API AAbilityPickUpVoxel : public AAbilityPickUpBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AAbilityPickUpVoxel();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void OnPickUp(IAbilityPickerInterface* InPicker) override;

public:
	virtual void Initialize(FAbilityItem InItem) override;
};
