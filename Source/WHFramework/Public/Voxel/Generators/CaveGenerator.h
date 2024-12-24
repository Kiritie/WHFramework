// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UVoxelModule;
class AVoxelChunk;

/**
 * 
 */
class WHFRAMEWORK_API CaveGenerator
{
public:
	static void GenerateCave(AVoxelChunk* chunk,UVoxelModule* info);
private:
	CaveGenerator() = delete;
	~CaveGenerator() = delete;
};
