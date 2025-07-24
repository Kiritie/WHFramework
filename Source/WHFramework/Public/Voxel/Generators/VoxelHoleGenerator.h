// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "Math/MathTypes.h"
#include "VoxelHoleGenerator.generated.h"

/**
 *
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelHoleGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	UVoxelHoleGenerator();

public:
	virtual void Generate(AVoxelChunk* InChunk) override;

protected:
	void GenerateHole(FIndex InIndex) const;

	FIndex FindNearestHoleIndex(FIndex InStartIndex, FVector InDirection, int32 InMaxSearchDistance) const;

	FIndex FindNearestHoleIndexMultiDirection(FIndex InStartIndex, int32 InMaxSearchDistance) const;

	TArray<FIndex> GenerateHoleSlopePath(FIndex InStartIndex, FIndex InEndIndex) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CrystalSize;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpawnRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxRadius;
};
