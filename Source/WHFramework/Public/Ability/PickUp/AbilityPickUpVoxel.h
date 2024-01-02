// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "AbilityPickUpBase.h"

#include "AbilityPickUpVoxel.generated.h"

class UVoxelMeshComponent;
/**
 * 可拾取体素
 */
UCLASS()
class WHFRAMEWORK_API AAbilityPickUpVoxel : public AAbilityPickUpBase
{
	GENERATED_BODY()

public:
	AAbilityPickUpVoxel();

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UVoxelMeshComponent* MeshComponent;

public:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

protected:
	virtual void OnPickUp(IAbilityPickerInterface* InPicker) override;

public:
	virtual UMeshComponent* GetMeshComponent() const override;
};
