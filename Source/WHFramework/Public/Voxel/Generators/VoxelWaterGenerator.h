// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "VoxelWaterGenerator.generated.h"

/**
 *
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelWaterGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	UVoxelWaterGenerator();

public:
	virtual void Generate(AVoxelChunk* InChunk) override;

public:
	void Flow(AVoxelChunk* InChunk, float InRain, int InX, int InY, int InZ);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	TSet<uint64> Waters;
};
