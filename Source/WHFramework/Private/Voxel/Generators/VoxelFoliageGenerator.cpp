// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelFoliageGenerator.h"

#include "Math/MathHelper.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelFoliageGenerator::UVoxelFoliageGenerator()
{
	Seed = 1317;
	CrystalSize = 16.f;
	GrassRate = 0.9f;
	FlowerRate = 0.98f;
	TreeRate = 0.99f;
	
	_Seed = 0;
}

void UVoxelFoliageGenerator::Generate(AVoxelChunk* InChunk)
{
	_Seed = FMathHelper::Hash21(InChunk->GetIndex().ToVector2D()) + Seed;
	
	ITER_INDEX2D(Index, Module->GetWorldData().ChunkSize, false,
		const FVoxelTopography& Topography = InChunk->GetTopography(Index);
		if(Topography.BiomeType == EVoxelBiomeType::None || Topography.BiomeType == EVoxelBiomeType::Stone || Topography.BiomeType == EVoxelBiomeType::Desert) continue;

		const FIndex GlobalIndex = InChunk->LocalIndexToWorld(Index);
		if(!Module->HasVoxelByIndex(FIndex(GlobalIndex.X, GlobalIndex.Y, Topography.Height), true) || Module->GetVoxelByIndex(FIndex(GlobalIndex.X, GlobalIndex.Y, Topography.Height)).GetVoxelType() == EVoxelType::Water) continue;

		if(Module->HasVoxelByIndex(FIndex(GlobalIndex.X, GlobalIndex.Y, Topography.Height + 1), true)) continue;

		if(GenerateTree(InChunk, Index, CrystalSize)) continue;

		GeneratePlant(InChunk, Index, CrystalSize);
	)
}

bool UVoxelFoliageGenerator::GeneratePlant(AVoxelChunk* InChunk, FIndex InIndex, int32 InCrystalSize)
{
	const FVoxelTopography& Topography = InChunk->GetTopography(InIndex);
	
	if(Topography.Height <= Module->GetWorldData().SeaLevel) return false;

	const float Temperature = Topography.Temperature;
	const float Humidity = Topography.Humidity;

	const FVector2D Location = FVector2D(float(InIndex.X) / Module->GetWorldData().ChunkSize.X / InCrystalSize, float(InIndex.Y) / Module->GetWorldData().ChunkSize.Y / InCrystalSize);
	const float Possible = FMathHelper::HashRand(Location, _Seed) - FMath::Abs(Temperature + 0.1f) * 0.5f + Humidity * 0.4f;

	EVoxelType VoxelType = EVoxelType::Empty;
	if(Possible > FlowerRate)
	{
		VoxelType = (EVoxelType)((int32)EVoxelType::Flower_Allium + FMathHelper::HashRandInt(FVector2D(InIndex.X + InIndex.Y * 21, InIndex.Y - InIndex.X ^ 2), _Seed) % ((int32)EVoxelType::Flower_Tulip_White -(int32)EVoxelType::Flower_Allium));
	}
	else if(Possible > GrassRate)
	{
		if(Temperature > 0.3f)
		{
			VoxelType = EVoxelType::Tall_Grass;
		}
		else if(Temperature > -0.3f)
		{
			VoxelType = EVoxelType::Tall_Grass;
		}
		else
		{
			VoxelType = EVoxelType::Tall_Grass;
		}
	}
	if(VoxelType != EVoxelType::Empty)
	{
		Module->SetVoxelByIndex(InChunk->LocalIndexToWorld(FIndex(InIndex.X, InIndex.Y, Topography.Height + 1)), VoxelType);
	}
	return true;
}

bool UVoxelFoliageGenerator::GenerateTree(AVoxelChunk* InChunk, FIndex InIndex, int32 InCrystalSize)
{
	const FVoxelTopography& Topography = InChunk->GetTopography(InIndex);
	
	if(Topography.Height <= Module->GetWorldData().SeaLevel - 1) return false;

	const FVector2D Location = FVector2D(float(InIndex.X) / Module->GetWorldData().ChunkSize.X / InCrystalSize, float(InIndex.Y) / Module->GetWorldData().ChunkSize.Y / InCrystalSize);
	float Temperature = Topography.Temperature;
	const float Humidity = Topography.Humidity;
	const float Possible = FMathHelper::HashRand(-Location, _Seed) * 0.9f + Module->GetVoxelNoise2D(Location, false, true) * 0.15f - FMath::Abs(Temperature + 0.1f) * 0.1f + Humidity * 0.15f;

	if(Possible <= TreeRate) return false;
	const int32 TreeHeight = (FMathHelper::HashRandInt(FVector2D::UnitVector - Location, _Seed) % 3) + 4;

	Temperature += (FMathHelper::HashRand(FVector2D::UnitVector + Location, _Seed) - 0.5f) * 0.2f;

	EVoxelType WoodType;
	EVoxelType LeafType;
	if(Temperature > 0.3f)
	{
		WoodType = EVoxelType::Oak;
		LeafType = EVoxelType::Oak_Leaves;
	}
	else if(Temperature > -0.3f)
	{
		WoodType = EVoxelType::Oak;
		LeafType = EVoxelType::Oak_Leaves;
	}
	else
	{
		WoodType = EVoxelType::Birch;
		LeafType = EVoxelType::Birch_Leaves;
	}

	for(int i = 0; i < TreeHeight; ++i)
	{
		Module->SetVoxelByIndex(InChunk->LocalIndexToWorld(FIndex(InIndex.X, InIndex.Y, Topography.Height + 1 + i)), WoodType);
	}

	const int32 T1 = FMathHelper::HashRand(17 * Location, _Seed) * 4 + 1.5f + int32(TreeHeight >= 5);
	const int32 T2 = FMathHelper::HashRand(11 * Location, _Seed) * 4 + 1.5f + int32(TreeHeight >= 5);
	const int32 LeafHeight = TreeHeight + 1 + T1 % 3;
	const int32 InitLeafHeight = 2 + T2 % 2;
	for(int i = LeafHeight - 1; i >= InitLeafHeight; --i)
	{
		const int32 LeafRadius = FMath::Clamp(FMathHelper::Bezier(FVector2D(0, 0), FVector2D(0.33f, T1), FVector2D(0.66f, T2), FVector2D(1, 0), float(i - InitLeafHeight) / (LeafHeight - 1 - InitLeafHeight)).Y, 0.f, 1.f) * 5;
		GenerateLeaves(InChunk, InIndex, Topography.Height + 1 + i, LeafRadius, LeafType);
	}
	return true;
}

void UVoxelFoliageGenerator::GenerateLeaves(AVoxelChunk* InChunk, FIndex InIndex, int32 InHeight, int32 InRadius, EVoxelType InLeafType)
{
	for(int i = 0; i < InRadius; ++i)
	{
		for(int j = 0; j < InRadius; ++j)
		{
			const FIndex Index =  InChunk->LocalIndexToWorld(FIndex(InIndex.X + i - InRadius / 2, InIndex.Y + j - InRadius / 2, InHeight));
			if(!Module->HasVoxelByIndex(Index, true))
			{
				Module->SetVoxelByIndex(Index, InLeafType);
			}
		}
	}
}
