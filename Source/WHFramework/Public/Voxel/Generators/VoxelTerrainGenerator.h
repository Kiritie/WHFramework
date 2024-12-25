// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "VoxelTerrainGenerator.generated.h"

/**
 *
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelTerrainGenerator : public UVoxelGenerator
{
	GENERATED_BODY()
	
public:
	UVoxelTerrainGenerator();
	
public:
	virtual void Generate(AVoxelChunk* InChunk) override;
};
