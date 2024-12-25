// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "VoxelTemperatureGenerator.generated.h"

/**
 *
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelTemperatureGenerator : public UVoxelGenerator
{
	GENERATED_BODY()
	
public:
	UVoxelTemperatureGenerator();
	
public:
	virtual void Generate(AVoxelChunk* InChunk) override;
};
  
