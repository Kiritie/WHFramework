// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelCaveGenerator.h"

#include "Math/MathStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelCaveGenerator::UVoxelCaveGenerator()
{
	Times = 3;
	MaxDepth = 5;
}

void UVoxelCaveGenerator::Generate(AVoxelChunk* InChunk)
{
	ITER_INDEX2D(Index, Module->GetWorldData().ChunkSize, false,
		const int32 Height = InChunk->GetTopography(Index).Height - 10;
		for(int Z = FMath::Max(Height - MaxDepth, 0); Z <= Height; Z++)
		{
			const FIndex WorldIndex = InChunk->LocalIndexToWorld(FIndex(Index.X, Index.Y, Z));
			if (IsCave(WorldIndex))
			{
				Module->SetVoxelByIndex(WorldIndex, FVoxelItem::Empty);
			}
		}
	)
}

bool UVoxelCaveGenerator::IsCave(FIndex InIndex) const
{
	if(InIndex.Z <= 0) return false;
	
	const float Scale = 0.05f;
	const float Persistence = 0.5f;

	float TotalNoise = 0.f;
	float Frequency = Scale;
	float Amplitude = 1.f;

	for (int i = 0; i < Times; i++)
	{
		TotalNoise += Module->GetVoxelNoise3D(FVector(InIndex.X * Frequency, InIndex.Y * Frequency, InIndex.Z * Frequency)) * Amplitude;

		Frequency *= 2.f;
		Amplitude *= Persistence;
	}

	return TotalNoise > 0.3f;
}
