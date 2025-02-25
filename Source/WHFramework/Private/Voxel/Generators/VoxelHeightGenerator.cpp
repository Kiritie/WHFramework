// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelHeightGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelHeightGenerator::UVoxelHeightGenerator()
{
	GenerateDatas = {
		FVoxelHeightGenerateData(6.f, 64, 0.2f),
		FVoxelHeightGenerateData(12.f, 64, 0.25f),
		FVoxelHeightGenerateData(32.f, 64, 0.3f)
	};
}

void UVoxelHeightGenerator::Generate(AVoxelChunk* InChunk)
{
	for(auto& Iter : GenerateDatas)
	{
		ITER_INDEX2D(Index, Module->GetWorldData().ChunkSize, false,
			const FVector2D Location = FVector2D((float)Index.X / Module->GetWorldData().ChunkSize.X / Iter.CrystalSize, Index.Y / Module->GetWorldData().ChunkSize.Y / Iter.CrystalSize);
			const int32 BiomeHeight = (Module->GetVoxelNoise2D(Location + InChunk->GetIndex().ToVector2D() / Iter.CrystalSize) * Iter.MaxHeight + Iter.MaxHeight) * Iter.Weight;
			FVoxelTopography Topography = InChunk->GetTopography(Index);
			Topography.Height += BiomeHeight;
			InChunk->SetTopography(Index, Topography);
		)
	}
}
