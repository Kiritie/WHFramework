// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "VoxelCaveGenerator.generated.h"

/**
 *
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelCaveGenerator : public UVoxelGenerator
{
	GENERATED_BODY()
	
public:
	UVoxelCaveGenerator();
	
public:
	virtual void Generate(AVoxelChunk* InChunk) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CrystalSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector NoiseScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxDepth;
};
