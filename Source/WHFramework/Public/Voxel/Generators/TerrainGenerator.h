// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UVoxelModule;
class AVoxelChunk;

/**
 * 生物群落分类器
 */
class WHFRAMEWORK_API TerrainGenerator
{
public:
	static void GenerateTerrain(AVoxelChunk* chunk,UVoxelModule* info);
private:
	TerrainGenerator() = delete;
	~TerrainGenerator() = delete;
};
