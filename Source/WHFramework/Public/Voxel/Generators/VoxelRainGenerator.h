// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "Math/MathTypes.h"
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
	virtual void Generate(UVoxelChunk* InChunk) override;

public:
	void Flow(UVoxelChunk* InChunk, float InRain, int InX, int InY, int InZ);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpawnRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxDistance;

private:
	TSet<FIndex> _Waters;
};
