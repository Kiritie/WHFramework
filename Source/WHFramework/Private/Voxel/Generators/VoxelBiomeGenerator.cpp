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
        float Temperature = InChunk->GetTopography(Index).Temperature;
        float Humidity = InChunk->GetTopography(Index).Humidity;

        EVoxelBiomeType VoxelBiome;

        if(Temperature < -0.2f)
			VoxelBiome = EVoxelBiomeType::Snow;
		else if(Temperature < 0.2f && Humidity < 0.05f)
			VoxelBiome = EVoxelBiomeType::Stone;
		else if(Temperature < 0.2f && Humidity < 0.1f)
			VoxelBiome = EVoxelBiomeType::Dry;
		else if(Temperature < 0.2f || Humidity > 0.7f)
			VoxelBiome = EVoxelBiomeType::Green;
		else
			VoxelBiome = EVoxelBiomeType::Desert;

        InChunk->GetTopography(Index).BiomeType = VoxelBiome;
    )
}
