// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelHeightGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelHeightGenerator::UVoxelHeightGenerator()
{
	GenerateDatas = {
		FVoxelHeightGenerateData(4, FVector(1.f, 1.f, 1.f), 16, 0.12f),
		FVoxelHeightGenerateData(16, FVector(1.f, 1.f, 1.f), 24, 0.22f),
		FVoxelHeightGenerateData(64, FVector(1.f, 1.f, 1.f), 32, 0.66f)
	};
}

void UVoxelHeightGenerator::Generate(AVoxelChunk* InChunk)
{
	for(auto& Iter : GenerateDatas)
	{
		ITER_INDEX2D(Index, Module->GetWorldData().ChunkSize, false,
			// const int32 BiomeHeight = Module->GetTerrainHeight(InChunk->LocalIndexToWorld(Index));
			const FVector2D Location = FVector2D((float)Index.X / Module->GetWorldData().ChunkSize.X / Iter.CrystalSize, Index.Y / Module->GetWorldData().ChunkSize.Y / Iter.CrystalSize);
			const int32 BiomeHeight = (Module->GetNoise2D(Location + InChunk->GetIndex().ToVector2D() / Iter.CrystalSize, Iter.NoiseScale) * Iter.MaxHeight + Iter.MaxHeight) * Iter.Weight;
			InChunk->GetTopography(Index).Height += BiomeHeight;
		)
	}
}
