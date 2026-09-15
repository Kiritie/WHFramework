// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelEntity.h"

#include "VoxelEntityCapture.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API AVoxelEntityCapture : public AVoxelEntity
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AVoxelEntityCapture();

protected:
	virtual void LoadData(const FParameter& InSaveData, EPhase InPhase) override;

public:
	virtual void SpawnAuxiliary() override;

	virtual void DestroyAuxiliary() override;
};
