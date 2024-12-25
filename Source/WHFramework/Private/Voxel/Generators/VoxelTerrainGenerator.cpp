// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Generators/VoxelTerrainGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelTerrainGenerator::UVoxelTerrainGenerator()
{
}

void UVoxelTerrainGenerator::Generate(AVoxelChunk* InChunk)
{
	//载入地形方块
	const FVector Range = FVector(Module->GetWorldData().ChunkSize.X, Module->GetWorldData().ChunkSize.Y, Module->GetWorldData().ChunkSize.Z * Module->GetWorldData().WorldSize.Z);
	ITER_INDEX(Index, Range, false,
		const FIndex _Index = InChunk->LocalIndexToWorld(Index);
		if(!Module->HasVoxelByIndex(_Index))
		{
			const EVoxelType VoxelType = Module->CaculateBlockID(_Index);
			if(VoxelType != EVoxelType::Empty)
			{
				Module->SetVoxelByIndex(_Index, VoxelType);
			}
		}
		else
		{
			const FVoxelItem VoxelItem = Module->GetVoxelByIndex(_Index);
			if(!VoxelItem.IsValid())
			{
				Module->SetVoxelByIndex(_Index, FVoxelItem::Empty, true);
			}
		}
	)
}
