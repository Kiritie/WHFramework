// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UVoxelModule;
class AVoxelChunk;

/**
 * 
 */
class WHFRAMEWORK_API HumidityGenerator
{
public:
	static void GenerateHumidity(AVoxelChunk* chunk,UVoxelModule* info);
private:
	HumidityGenerator() = delete;
	~HumidityGenerator() = delete;
};
