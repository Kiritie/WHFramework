// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UVoxelModule;
class AVoxelChunk;

/**
 * 
 */
class WHFRAMEWORK_API TemperatureGenerator
{
public:
	static void GenerateTemperature(AVoxelChunk* chunk,UVoxelModule* info);
private:
	TemperatureGenerator() = delete;
	~TemperatureGenerator() = delete;
};
  
