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
		FIndex _Index = InChunk->LocalIndexToWorld(Index);
		DON_WITHINDEX(FMath::Max(Module->GetTopographyByIndex(_Index).Height, Module->GetWorldData().SeaLevel) + 1, Z,
			_Index.Z = Z;
			if(!Module->HasVoxelByIndex(_Index))
			{
				const EVoxelType VoxelType = CalculateVoxelType(_Index);
				if(VoxelType != EVoxelType::Empty)
				{
					Module->SetVoxelByIndex(_Index, VoxelType);
				}
			}
			else if(!Module->GetVoxelByIndex(_Index).IsValid())
			{
				Module->SetVoxelByIndex(_Index, FVoxelItem::Empty, true);
			}
		)
	)
}

EVoxelType UVoxelTerrainGenerator::CalculateVoxelType(FIndex InIndex) const
{
	const FVoxelTopography& Topography = Module->GetTopographyByIndex(InIndex);

	if(InIndex.Z < BaseHeight) return EVoxelType::Bedrock;

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
