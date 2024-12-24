// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UVoxelModule;
class AVoxelChunk;

/**
 * 
 */
class WHFRAMEWORK_API RainGenerator
{
public:
	static void GenerateRain(AVoxelChunk* chunk,UVoxelModule* info);
	static void flow(AVoxelChunk* chunk,UVoxelModule* info,float rain,int i,int j,int k);
	static TSet<uint64> waters;
private:
	RainGenerator() = delete;
	~RainGenerator() = delete;
};
