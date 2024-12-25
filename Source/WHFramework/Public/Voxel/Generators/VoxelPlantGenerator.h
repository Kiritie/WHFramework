// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "Voxel/VoxelModuleTypes.h"
#include "VoxelPlantGenerator.generated.h"

/**
 *
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelPlantGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	UVoxelPlantGenerator();

public:
	//生成植物
	virtual void Generate(AVoxelChunk* InChunk) override;

public:
	//生成花
	bool GenerateFlower(AVoxelChunk* InChunk, int32 InX, int32 InY, int32 InCystalSize);

	//生成树
	bool GenerateTree(AVoxelChunk* InChunk, int32 InX, int32 InY, int32 InCystalSize);

	//生成树叶
	void GenerateLeaves(AVoxelChunk* InChunk, int32 InX, int32 InY, int32 InHeight, int32 InRadius, EVoxelType InTargetLeafID);

private:
	//树叶模板（生成树叶阶段用）
	static bool LeavesTemplate[4][5][5];
};
