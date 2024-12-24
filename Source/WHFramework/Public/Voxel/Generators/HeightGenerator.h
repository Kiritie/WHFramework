// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UVoxelModule;
class AVoxelChunk;

/**
 * 
 */
class WHFRAMEWORK_API HeightGenerator
{
public:
	static void GenerateHeight(AVoxelChunk* chunk,UVoxelModule* info);
private:
	HeightGenerator() = delete;
	~HeightGenerator() = delete;
};
