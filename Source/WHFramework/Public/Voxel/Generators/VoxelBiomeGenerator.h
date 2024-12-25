// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "VoxelBiomeGenerator.generated.h"

/**
 *
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelBiomeGenerator : public UVoxelGenerator
{
	GENERATED_BODY()
	
public:
	UVoxelBiomeGenerator();
	
public:
	virtual void Generate(AVoxelChunk* InChunk) override;
};
