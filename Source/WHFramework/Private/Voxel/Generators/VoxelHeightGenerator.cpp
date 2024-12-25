// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelHeightGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelHeightGenerator::UVoxelHeightGenerator()
{
}

void UVoxelHeightGenerator::Generate(AVoxelChunk* InChunk)
{
	//晶格大小
	int32 CystalSize[3] = {4, 16, 64};
	//权重
	float Weight[3] = {0.12f, 0.22f, 0.66f};
	//波幅
	float MaxHeight[3] = {16.0f, 24.0f, 32.0f};

	DON_WITHINDEX(3, N,
		ITER_INDEX2D(Index, Module->GetWorldData().ChunkSize, false,
			// const int32 BiomeHeight = Module->GetTerrainHeight(InChunk->LocalIndexToWorld(Index));
			const FVector2D Location = FVector2D(Index.X / Module->GetWorldData().ChunkSize.X / CystalSize[N], Index.Y / Module->GetWorldData().ChunkSize.Y / CystalSize[N]);
			const int32 BiomeHeight = (Module->GetNoise2D(Location + InChunk->GetIndex().ToVector2D() / CystalSize[N], FVector(1.f, 1.f, 1.f)) * MaxHeight[N] + MaxHeight[N]) * Weight[N];
			InChunk->GetTopography(Index).Height += BiomeHeight;
		)
	)
}
