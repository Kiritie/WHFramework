// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "VoxelHumidityGenerator.generated.h"

/**
 *
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelHumidityGenerator : public UVoxelGenerator
{
	GENERATED_BODY()
	
public:
	UVoxelHumidityGenerator();
	
public:
	virtual void Generate(AVoxelChunk* InChunk) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Times;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CrystalSize;
};
