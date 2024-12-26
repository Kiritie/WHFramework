// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Generators/VoxelHumidityGenerator.h"

#include "Math/MathStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelHumidityGenerator::UVoxelHumidityGenerator()
{
	Seed = 201;
	Times = 3;
	CrystalSize  = 16;
	NoiseScale = FVector(1.f, 1.f, 1.f);
}

void UVoxelHumidityGenerator::Generate(AVoxelChunk* InChunk)
{
	int32 _CrystalSize  = CrystalSize;

	DON_WITHINDEX(Times, N,
		ITER_INDEX2D(Index, Module->GetWorldData().ChunkSize, false,
			const FVector2D Location = FVector2D((float)Index.X / Module->GetWorldData().ChunkSize.X / CrystalSize, Index.Y / Module->GetWorldData().ChunkSize.Y / CrystalSize);
			const float Humidity = FMath::Clamp(Module->GetNoise2D(Location + (InChunk->GetIndex().ToVector2D() / CrystalSize), NoiseScale, true) + UMathStatics::Rand(Location, Seed) * 0.05f, 0.f, 1.f);
			InChunk->GetTopography(Index).Humidity += Humidity / Times;
		)
		_CrystalSize *= 2;
	)
}
