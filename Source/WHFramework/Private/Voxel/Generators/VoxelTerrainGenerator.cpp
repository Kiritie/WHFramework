// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Generators/VoxelTerrainGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelTerrainGenerator::UVoxelTerrainGenerator()
{
	BaseHeight = 1;
	UnderDepth = 3;
}

void UVoxelTerrainGenerator::Generate(AVoxelChunk* InChunk)
{
	//载入地形方块
	ITER_INDEX2D(Index, Module->GetWorldData().ChunkSize, false,
		DON_WITHINDEX(FMath::Max(InChunk->GetTopography(Index).Height, Module->GetWorldData().SeaLevel) + 1, Z,
			FIndex _Index = FIndex(Index.X, Index.Y, Z);
			if(!InChunk->HasVoxel(_Index))
			{
				const EVoxelType VoxelType = CalculateVoxelType(InChunk, _Index);
				if(VoxelType != EVoxelType::Empty)
				{
					InChunk->SetVoxel(_Index, VoxelType);
				}
			}
			else if(!InChunk->GetVoxel(_Index).IsValid())
			{
				if(_Index.Z < BaseHeight)
				{
					InChunk->SetVoxel(_Index, EVoxelType::Bedrock);
				}
				else
				{
					InChunk->SetVoxel(_Index, FVoxelItem::Empty, true);
				}
			}
		)
	)
}

EVoxelType UVoxelTerrainGenerator::CalculateVoxelType(AVoxelChunk* InChunk, FIndex InIndex) const
{
	if(InIndex.Z < BaseHeight) return EVoxelType::Bedrock;

	const FVoxelTopography& Topography = InChunk->GetTopography(FIndex(InIndex.X, InIndex.Y));

	const int32 SeaLevel = Module->GetWorldData().SeaLevel;

	const int32 Depth = Topography.Height - InIndex.Z;
	
	if(Depth <= 0) //地表方块
	{
		if(InIndex.Z <= SeaLevel)
		{
			if(Depth == 0)
			{
				return GetBiomeVoxelType(Topography.BiomeType, InIndex.Z < SeaLevel);
			}
			return EVoxelType::Water;
		}
		else if(Depth == 0)
		{
			return GetBiomeVoxelType(Topography.BiomeType, false);
		}
		return EVoxelType::Empty;
	}
	else if(Depth <= UnderDepth) //地下方块
	{
		return GetBiomeVoxelType(Topography.BiomeType, true);
	}
	else
	{
		return EVoxelType::Stone; //地下石头
	}
}

EVoxelType UVoxelTerrainGenerator::GetBiomeVoxelType(EVoxelBiomeType InBiomeType, bool bUnderGround) const
{
	if(!bUnderGround)
	{
		switch(InBiomeType)
		{
			case EVoxelBiomeType::Snow:		return EVoxelType::Snow;
			case EVoxelBiomeType::Green:	return EVoxelType::Grass;
			case EVoxelBiomeType::Dry:		return EVoxelType::Dirt;
			case EVoxelBiomeType::Stone:	return EVoxelType::Stone;
			case EVoxelBiomeType::Desert:	return EVoxelType::Sand;
			default:						return EVoxelType::Empty;
		}
	}
	else
	{
		switch(InBiomeType)
		{
			case EVoxelBiomeType::Snow:		return EVoxelType::Dirt;
			case EVoxelBiomeType::Green:	return EVoxelType::Dirt;
			case EVoxelBiomeType::Dry:		return EVoxelType::Dirt;
			case EVoxelBiomeType::Stone:	return EVoxelType::Stone;
			case EVoxelBiomeType::Desert:	return EVoxelType::Sand;
			default:						return EVoxelType::Empty;
		}
	}
}
