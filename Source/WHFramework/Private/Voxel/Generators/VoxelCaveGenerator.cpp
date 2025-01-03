// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelCaveGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelCaveGenerator::UVoxelCaveGenerator()
{
	CrystalSize = 2.f;
	MaxDepth = 5;
}

void UVoxelCaveGenerator::Generate(AVoxelChunk* InChunk)
{
    ITER_INDEX2D(Index, Module->GetWorldData().ChunkSize, false,
		const int32 Height = InChunk->GetTopography(Index).Height;
		for(int Z = Height; Z >= FMath::Max(Height - MaxDepth, 0); Z--)
		{
			if(Z <= Module->GetWorldData().SeaLevel) break;
			
			const FVector Location = FVector((float)Index.X / Module->GetWorldData().ChunkSize.X / CrystalSize, (float)Index.Y / Module->GetWorldData().ChunkSize.Y / CrystalSize, (float)Z / Module->GetWorldData().ChunkSize.Z / CrystalSize);
			//若高于一定阈值，挖空
			if ((Module->GetVoxelNoise3D(Location + InChunk->GetIndex().ToVector() / CrystalSize) + (float)(Height - Z) / MaxDepth * 0.3f) > 0.5f)
			{
				Module->SetVoxelByIndex(InChunk->LocalIndexToWorld(FIndex(Index.X, Index.Y, Z)), FVoxelItem::Empty);
			}
		}
	)
}
