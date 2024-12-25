// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelTemperatureGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelTemperatureGenerator::UVoxelTemperatureGenerator()
{
}

void UVoxelTemperatureGenerator::Generate(AVoxelChunk* InChunk)
{
	const int32 Times = 3;
	int32 CystalSize  = 16;

	DON_WITHINDEX(Times, N,
		ITER_INDEX2D(Index, Module->GetWorldData().ChunkSize, false,
			const FVector2D Location = FVector2D(Index.X / Module->GetWorldData().ChunkSize.X / CystalSize, Index.Y / Module->GetWorldData().ChunkSize.Y / CystalSize);
			const float Temperature = FMath::Clamp(Module->GetNoise2D(Location + (InChunk->GetIndex().ToVector2D() / CystalSize), FVector(1.f, 1.f, 1.f), true), 0.f, 1.f);
			InChunk->GetTopography(Index).Temperature += Temperature / Times;
		)
		CystalSize *= 2;
	)
}
