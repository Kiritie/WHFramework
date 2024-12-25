// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "VoxelRainGenerator.generated.h"

/**
 *
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelRainGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	UVoxelRainGenerator();

public:
	virtual void Generate(AVoxelChunk* InChunk) override;

public:
	void Flow(AVoxelChunk* InChunk, float InRain, int InX, int InY, int InZ);

protected:
	TSet<uint64> Waters;
};
