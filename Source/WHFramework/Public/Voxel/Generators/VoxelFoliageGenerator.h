// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "Voxel/VoxelModuleTypes.h"
#include "VoxelFoliageGenerator.generated.h"

/**
 *
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelFoliageGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	UVoxelFoliageGenerator();

public:
	virtual void Generate(AVoxelChunk* InChunk) override;

public:
	//生成植物
	bool GeneratePlant(AVoxelChunk* InChunk, FIndex InIndex, int32 InCrystalSize);

	//生成树
	bool GenerateTree(AVoxelChunk* InChunk, FIndex InIndex, int32 InCrystalSize);

	//生成树叶
	void GenerateLeaves(AVoxelChunk* InChunk, FIndex InIndex, int32 InHeight, int32 InRadius, EVoxelType InLeafType);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CrystalSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GrassRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FlowerRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TreeRate;

private:
	int32 _Seed;
};
