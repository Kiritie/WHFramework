

#include "Voxel/Generators/VoxelTerrainGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelTerrainGenerator::UVoxelTerrainGenerator()
{
	BaseHeight = 1;
	UnderDepth = 3;
}

void UVoxelTerrainGenerator::Generate(UVoxelChunk* InChunk)
{
	//载入地形方块
	ITER_INDEX2D(Index, Module->GetWorldData().ChunkSize, false,
		const FVoxelTopography& Topography = InChunk->GetTopography(Index);
		const int32 TopographyHeight = Topography.Height;
		const int32 WaterHeight = Topography.WaterHeight;
		const bool bRiverSand = Topography.RegionType == EVoxelWorldRegionType::River && Topography.BiomeType == EVoxelBiomeType::Desert;
		const int32 ColumnHeight = FMath::Max3(TopographyHeight, Module->GetWorldData().SeaLevel, WaterHeight);
		DON_WITHINDEX(ColumnHeight + 1, Z,
			const FIndex _Index = FIndex(Index.X, Index.Y, Z);
			if(WaterHeight != INDEX_NONE && Z >= TopographyHeight + (bRiverSand ? 1 : 0) && Z <= WaterHeight)
			{
				if(!InChunk->HasVoxel(_Index, true) || InChunk->GetVoxel(_Index).GetVoxelType() != EVoxelType::Water)
				{
					InChunk->SetVoxel(_Index, EVoxelType::Water);
				}
				continue;
			}
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

EVoxelType UVoxelTerrainGenerator::CalculateVoxelType(UVoxelChunk* InChunk, FIndex InIndex) const
{
	if(InIndex.Z < BaseHeight) return EVoxelType::Bedrock;

	const FVoxelTopography& Topography = InChunk->GetTopography(FIndex(InIndex.X, InIndex.Y));

	const int32 SeaLevel = Module->GetWorldData().SeaLevel;
	const int32 WaterHeight = Topography.WaterHeight;
	const bool bRiverSand = Topography.RegionType == EVoxelWorldRegionType::River && Topography.BiomeType == EVoxelBiomeType::Desert;
	if(WaterHeight != INDEX_NONE && InIndex.Z >= Topography.Height + (bRiverSand ? 1 : 0) && InIndex.Z <= WaterHeight)
	{
		return EVoxelType::Water;
	}

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
	switch(InBiomeType)
	{
		case EVoxelBiomeType::Snow:		return bUnderGround ? EVoxelType::Dirt : EVoxelType::Snow;
		case EVoxelBiomeType::Green:	return bUnderGround ? EVoxelType::Dirt : EVoxelType::Grass;
		case EVoxelBiomeType::Dry:		return EVoxelType::Dirt;
		case EVoxelBiomeType::Stone:	return EVoxelType::Stone;
		case EVoxelBiomeType::Desert:	return EVoxelType::Sand;
		case EVoxelBiomeType::Ocean:	return EVoxelType::Sand;
		case EVoxelBiomeType::River:	return EVoxelType::Dirt;
		case EVoxelBiomeType::Plains:	return bUnderGround ? EVoxelType::Dirt : EVoxelType::Grass;
		case EVoxelBiomeType::Forest:	return bUnderGround ? EVoxelType::Dirt : EVoxelType::Grass;
		case EVoxelBiomeType::Taiga:	return bUnderGround ? EVoxelType::Dirt : EVoxelType::Grass;
		case EVoxelBiomeType::Savanna:	return bUnderGround ? EVoxelType::Dirt : EVoxelType::Grass;
		case EVoxelBiomeType::Swamp:	return bUnderGround ? EVoxelType::Dirt : EVoxelType::Grass;
		case EVoxelBiomeType::Mountains:return EVoxelType::Stone;
		case EVoxelBiomeType::Badlands:	return EVoxelType::Sand_Stone;
		case EVoxelBiomeType::Hills:		return bUnderGround ? EVoxelType::Dirt : EVoxelType::Grass;
		default:						return EVoxelType::Empty;
	}
}
