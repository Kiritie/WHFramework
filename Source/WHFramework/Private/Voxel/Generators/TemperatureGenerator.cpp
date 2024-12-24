// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/TemperatureGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

void TemperatureGenerator::GenerateTemperature(AVoxelChunk* chunk,UVoxelModule* info)
{
	const int32 times = 3;
	int32 cystalSize  = 16;

	for (int d = 0; d < times; ++d)
	{
		for (int i = 0; i < UVoxelModule::Get().GetWorldData().ChunkSize.X; ++i)
		{
			for (int j = 0; j < UVoxelModule::Get().GetWorldData().ChunkSize.Y; ++j)
			{
				FVector2D pf = FVector2D((float)i / UVoxelModule::Get().GetWorldData().ChunkSize.X / cystalSize, float(j) / UVoxelModule::Get().GetWorldData().ChunkSize.Y / cystalSize);
	
				float value = FMath::Clamp(UVoxelModule::Get().GetNoise2D(pf + (chunk->GetIndex().ToVector2D() / cystalSize), FVector(1.f, 1.f, 1.f), 143, false),-1.f,1.0f);
	
				chunk->GetTopography(FIndex(i, j)).Temperature += value / times;
			}
		}
		cystalSize*=2;
	}
}
