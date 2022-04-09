// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "PickUp.h"

#include "PickUpEquip.generated.h"

/**
 * ��ʰȡװ��
 */
UCLASS()
class WHFRAMEWORK_API APickUpEquip : public APickUp
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	APickUpEquip();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void OnPickUp(IPickerInterface* InPicker) override;

public:
	virtual void Initialize(FAbilityItem InItem, bool bPreview = false) override;
};
