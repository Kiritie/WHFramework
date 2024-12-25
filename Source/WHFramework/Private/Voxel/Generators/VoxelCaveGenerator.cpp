// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelCaveGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelCaveGenerator::UVoxelCaveGenerator()
{
}

void UVoxelCaveGenerator::Generate(AVoxelChunk* InChunk)
{
	//晶格大小
	int32 CystalSize = 2;

    ITER_INDEX2D(Index, Module->GetWorldData().ChunkSize, false,
		int32 Height = InChunk->GetTopography(Index).Height;
		for(int Z = Height; Z >= FMath::Max(Height - 10, 0); Z--)
		{
			FVector Location = FVector(
				Index.X / Module->GetWorldData().ChunkSize.X / CystalSize,
				Index.Y / Module->GetWorldData().ChunkSize.Y / CystalSize,
				(float)Z / Module->GetWorldData().ChunkSize.Z / CystalSize
			);

			//若高于一定阈值，挖空
			if ((Module->GetNoise3D(Location + FVector(InChunk->GetIndex().X, InChunk->GetIndex().Y, 0) / 2, FVector(1.f, 1.f, 1.f)) + (Height - Z) / 10.0f * 0.3f) > 0.5f)
			{
				Module->SetVoxelByIndex(InChunk->LocalIndexToWorld(FIndex(Index.X, Index.Y, Z)), FVoxelItem::Empty);
			}

			Height = FMath::Max(Height, Z);
		}
		InChunk->GetTopography(Index).Height = Height;
	)
}
