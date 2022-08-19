// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelEntity.h"

#include "VoxelEntityPreview.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API AVoxelEntityPreview : public AVoxelEntity
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AVoxelEntityPreview();

public:
	virtual void Initialize(FPrimaryAssetId InVoxelID) override;
};
