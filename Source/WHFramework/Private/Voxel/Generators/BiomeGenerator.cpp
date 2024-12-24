// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Generators/BiomeGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"

void BiomeGenerator::GenerateBiome(AVoxelChunk* chunk,UVoxelModule* info){
    for (int i = 0; i < UVoxelModule::Get().GetWorldData().ChunkSize.X; ++i)
	for (int j = 0; j < UVoxelModule::Get().GetWorldData().ChunkSize.Y; ++j)
	{
        float temperature = chunk->GetTopography(FIndex(i, j)).Temperature;
        float humidity = chunk->GetTopography(FIndex(i, j)).Humidity;

        EVoxelBiomeType targetBiome;

        if(temperature < -0.2f)
			targetBiome = EVoxelBiomeType::Snow;
		else if(temperature < 0.2f && humidity < 0.05f)
			targetBiome = EVoxelBiomeType::Stone;
		else if(temperature < 0.2f && humidity < 0.1f)
			targetBiome = EVoxelBiomeType::Dry;
		else if(temperature < 0.2f || humidity > 0.7f)
			targetBiome = EVoxelBiomeType::Green;
		else
			targetBiome = EVoxelBiomeType::Desert;

        chunk->GetTopography(FIndex(i, j)).BiomeType = targetBiome;
    }
}
