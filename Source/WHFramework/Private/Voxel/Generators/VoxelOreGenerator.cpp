// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelOreGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelOreGenerator::UVoxelOreGenerator()
{
	GenerateDatas = {
		FVoxelOreGenerateData(EVoxelType::Diamond_Ore, 0.1f, 10),
		FVoxelOreGenerateData(EVoxelType::Emerald_Ore, 0.1f, 20),
		FVoxelOreGenerateData(EVoxelType::Gold_Ore, 0.1f, 30),
		FVoxelOreGenerateData(EVoxelType::Iron_Ore, 0.15f, 50),
		FVoxelOreGenerateData(EVoxelType::Coal_Ore, 0.3f, -1),
	};
}

void UVoxelOreGenerator::Generate(AVoxelChunk* InChunk)
{
    ITER_INDEX2D(Index, Module->GetWorldData().ChunkSize, false,
		const int32 Height = InChunk->GetTopography(Index).Height;
		for(int Z = 0; Z < Height; Z++)
		{
			for(auto& Iter : GenerateDatas)
			{
				if(Iter.MaxHeight >= 0 && Z > Iter.MaxHeight) continue;
				
				const FVector Location = FVector((float)Index.X / Module->GetWorldData().ChunkSize.X / Iter.CrystalSize, (float)Index.Y / Module->GetWorldData().ChunkSize.Y / Iter.CrystalSize, (float)Z / Module->GetWorldData().ChunkSize.Z / Iter.CrystalSize);
				const FIndex WorldIndex = InChunk->LocalIndexToWorld(FIndex(Index.X, Index.Y, Z));
				if (!Module->HasVoxelByIndex(WorldIndex) && Module->GetVoxelNoise3D(Location + InChunk->GetIndex().ToVector() * 100.f) > 0.5f)
				{
					Module->SetVoxelByIndex(WorldIndex, Iter.VoxelType);
					break;
				}
			}
		}
	)
}
