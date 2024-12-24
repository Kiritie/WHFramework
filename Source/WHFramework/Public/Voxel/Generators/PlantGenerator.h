// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Voxel/VoxelModuleTypes.h"

class UVoxelModule;
class AVoxelChunk;

/**
 * 植被生成器
 */
class WHFRAMEWORK_API PlantGenerator
{
public:
	//生成树
	static void GeneratePlant(AVoxelChunk* chunk,UVoxelModule* info);
	//生成花
	static bool GenerateFlower(AVoxelChunk* chunk,UVoxelModule* info,int32 i,int32 j,int32 cystalSize);
	//生成树
	static bool GenerateTree(AVoxelChunk* chunk,UVoxelModule* info,int32 i,int32 j,int32 cystalSize);
	//生成树叶
	static void GenerateLeaves(AVoxelChunk* chunk,UVoxelModule* info,int32 i,int32 j,int32 height,int32 radius,EVoxelType targetLeafID);
private:
	PlantGenerator() = delete;
	~PlantGenerator() = delete;
private:
	//树叶模板（生成树叶阶段用）
	static bool leavesTemplate[4][5][5];
};
