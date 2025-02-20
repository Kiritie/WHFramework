// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "Math/MathTypes.h"
#include "VoxelLakeGenerator.generated.h"

/**
 *
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelLakeGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	UVoxelLakeGenerator();

public:
	virtual void Generate(AVoxelChunk* InChunk) override;

protected:
	void GenerateLake(FIndex InIndex);

	void GenerateLakeDepression(FIndex InIndex, float InRadius, float InDepth);

	void SmoothLakeEdges(FIndex InIndex, float InRadius, int32 InIterations);

	void FillLakeWater(FIndex InIndex, float InRadius);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpawnRate;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MinDistance;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxRadius;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinDepth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxDepth;
};
