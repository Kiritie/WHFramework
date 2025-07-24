// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "Voxel/VoxelModuleTypes.h"
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

public:
	virtual EVoxelType CalculateVoxelType(AVoxelChunk* InChunk, FIndex InIndex) const;

	virtual EVoxelType GetBiomeVoxelType(EVoxelBiomeType InBiomeType, bool bUnderGround) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 BaseHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 UnderDepth;
};
