// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelBiomeGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelBiomeGenerator::UVoxelBiomeGenerator()
{
}

void UVoxelBiomeGenerator::Generate(AVoxelChunk* InChunk)
{
	ITER_INDEX2D(Index, Module->GetWorldData().ChunkSize, false,
		FVoxelTopography Topography = InChunk->GetTopography(Index);

		if(Topography.Temperature < -0.2f)
		{
			Topography.BiomeType = EVoxelBiomeType::Snow;
		}
		else if(Topography.Temperature < 0.2f && Topography.Humidity < 0.05f)
		{
			Topography.BiomeType = EVoxelBiomeType::Stone;
		}
		else if(Topography.Temperature < 0.2f && Topography.Humidity < 0.1f)
		{
			Topography.BiomeType = EVoxelBiomeType::Dry;
		}
		else if(Topography.Temperature < 0.2f || Topography.Humidity > 0.7f)
		{
			Topography.BiomeType = EVoxelBiomeType::Green;
		}
		else
		{
			Topography.BiomeType = EVoxelBiomeType::Desert;
		}

		InChunk->SetTopography(Index, Topography);
	)
}
