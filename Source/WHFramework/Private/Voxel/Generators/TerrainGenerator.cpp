// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Generators/TerrainGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"

void TerrainGenerator::GenerateTerrain(AVoxelChunk* chunk,UVoxelModule* info){
	//载入地形方块
	for (int i = 0; i < info->GetWorldData().ChunkSize.X; ++i)
	{
		for (int j = 0; j < info->GetWorldData().ChunkSize.Y; ++j) 
		{	
			for (int z = 0; z < info->GetWorldData().ChunkSize.Z * info->GetWorldData().WorldSize.Z; ++z)
			{
				const FIndex Index = chunk->LocalIndexToWorld(FIndex(i, j, z));
				if(!info->HasVoxelByIndex(Index))
				{
					const EVoxelType VoxelType = info->CaculateBlockID(Index);
					if(VoxelType != EVoxelType::Empty)
					{
						info->SetVoxelByIndex(Index, VoxelType);
					}
				}
				else
				{
					const FVoxelItem VoxelItem = info->GetVoxelByIndex(Index);
					if(!VoxelItem.IsValid())
					{
						info->SetVoxelByIndex(Index, FVoxelItem::Empty, true);
					}
				}
			}
		}
	}
}
