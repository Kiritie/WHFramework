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
	GrassRate = 0.1f;
	FlowerRate = 0.02f;
	FlowerFixedRate = 0.01f;
	TreeRate = 0.04f;
	TreeFixedRate = 0.002f;
	TreeRandomRate = 0.2f;
	
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
	float Possible = FMathHelper::HashRand(Location, _Seed);
	Possible = (1.f - Possible) <= FlowerFixedRate ? (Possible * 0.9f + Module->GetVoxelNoise2D(Location, false, true) * 0.25f) : (Possible - FMath::Abs(Temperature + 0.1f) * 0.5f + Humidity * 0.4f);

	EVoxelType VoxelType = EVoxelType::Empty;
	if((1.f - Possible) <= FlowerRate)
	{
		VoxelType = (EVoxelType)((int32)EVoxelType::Flower_Allium + FMathHelper::HashRandInt(FVector2D(InIndex.X + InIndex.Y * 21, InIndex.Y - InIndex.X ^ 2), _Seed) % ((int32)EVoxelType::Flower_Tulip_White -(int32)EVoxelType::Flower_Allium));
	}
	else if((1.f - Possible) <= GrassRate)
	{
		VoxelType = EVoxelType::Tall_Grass;
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
	float Possible = FMathHelper::HashRand(-Location, _Seed);
	Possible = Possible * 0.9f + ((1.f - Possible) <= TreeFixedRate ? Module->GetVoxelNoise2D(Location, false, true) * 0.2f : (Module->GetVoxelNoise2D(Location, false, true) * 0.1f - FMath::Abs(Temperature + 0.1f) * 0.1f + Humidity * 0.1f));

	if((1.f - Possible) > TreeRate) return false;

	Possible = FMathHelper::HashRand(FVector2D::UnitVector + Location, _Seed);
	Temperature = (1.f - Possible) <= TreeRandomRate ? (Module->GetVoxelNoise2D(FVector2D::UnitVector + Location) * 0.2f - 0.3f) : (Temperature + (Possible - 0.5f) * 0.2f);

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

	const int32 TreeHeight = (FMathHelper::HashRandInt(FVector2D::UnitVector - Location, _Seed) % 3) + 4;

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
		const int32 LeafRadius = FMath::Clamp(FMathHelper::Bezier(FVector2D(0, 0), FVector2D(0.33f, T1), FVector2D(0.66f, T2), FVector2D(1, 0), float(i - InitLeafHeight) / (LeafHeight - 1 - InitLeafHeight)).Y, 0.5f, 1.f) * 5;
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
