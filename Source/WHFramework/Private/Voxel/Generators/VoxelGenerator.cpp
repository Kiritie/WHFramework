// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelGenerator.h"

UVoxelGenerator::UVoxelGenerator()
{
	Module = nullptr;
}

void UVoxelGenerator::Initialize(UVoxelModule* InModule)
{
	Module = InModule;
}

void UVoxelGenerator::Generate(AVoxelChunk* InChunk)
{
	
}
