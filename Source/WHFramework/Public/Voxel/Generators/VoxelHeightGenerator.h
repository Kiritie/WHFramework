// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "VoxelHeightGenerator.generated.h"

/**
 *
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelHeightGenerator : public UVoxelGenerator
{
	GENERATED_BODY()
	
public:
	UVoxelHeightGenerator();
	
public:
	virtual void Generate(AVoxelChunk* InChunk) override;
};
