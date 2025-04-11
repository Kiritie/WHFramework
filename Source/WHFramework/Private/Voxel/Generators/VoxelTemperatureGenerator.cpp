// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelTemperatureGenerator.h"

#include "Math/MathHelper.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelTemperatureGenerator::UVoxelTemperatureGenerator()
{
	Seed = 143;
	Times = 3;
	CrystalSize  = 64;
}

void UVoxelTemperatureGenerator::Generate(AVoxelChunk* InChunk)
{
	int32 _CrystalSize  = CrystalSize;

	DON_WITHINDEX(Times, N,
		ITER_INDEX2D(Index, Module->GetWorldData().ChunkSize, false,
			const FVector2D Location = FVector2D(Index.X / Module->GetWorldData().ChunkSize.X / _CrystalSize, Index.Y / Module->GetWorldData().ChunkSize.Y / _CrystalSize);
			const float Temperature = FMath::Clamp(Module->GetVoxelNoise2D(Location + InChunk->GetIndex().ToVector2D() / _CrystalSize) + (FMathHelper::HashRand(Location, Seed) - 0.5f) * 0.05f, -1.f, 1.f);
			FVoxelTopography Topography = InChunk->GetTopography(Index);
			Topography.Temperature += Temperature / Times;
			InChunk->SetTopography(Index, Topography);
		)
		_CrystalSize *= 2;
	)
}
