// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "DreamWorld/DreamWorld.h"
#include "PickUp/PickUp.h"
#include "PickUpVoxel.generated.h"

/**
 * 可拾取体素
 */
UCLASS()
class DREAMWORLD_API APickUpVoxel : public APickUp
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	APickUpVoxel();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void OnPickUp(AAbilityCharacterBase* InPicker) override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void Initialize(FItem InItem, bool bPreview = false) override;

	UFUNCTION(BlueprintPure)
	UVoxelAssetBase GetVoxelData() const;
};
