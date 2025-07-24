// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelOreGenerator.h"

#include "Math/MathHelper.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelOreGenerator::UVoxelOreGenerator()
{
	Seed = 34834;
	SpawnRate = 0.5f;
	GenerateDatas = {
		FVoxelOreGenerateData(EVoxelType::Diamond_Ore, 10, 0.01f, 1, 3),
		FVoxelOreGenerateData(EVoxelType::Emerald_Ore, 20, 0.03f, 1, 5),
		FVoxelOreGenerateData(EVoxelType::Gold_Ore, 30, 0.05f, 3, 8),
		FVoxelOreGenerateData(EVoxelType::Iron_Ore, 50, 0.1f, 5, 10),
		FVoxelOreGenerateData(EVoxelType::Coal_Ore, -1, 0.45f, 10, 20),
	};
}

void UVoxelOreGenerator::Generate(AVoxelChunk* InChunk)
{
    ITER_INDEX2D(Index, Module->GetWorldData().ChunkSize, false,
		const int32 Height = InChunk->GetTopography(Index).Height - 2;
		for(int Z = 0; Z < Height; Z++)
		{
			const FIndex WorldIndex = InChunk->LocalIndexToWorld(FIndex(Index.X, Index.Y, Z));
			ITER_ARRAY_WITHINDEX(GenerateDatas, i, Iter,
				if(Iter.MaxHeight != -1 && WorldIndex.Z > Iter.MaxHeight) continue;

				const float Noise = Module->GetVoxelNoise3D(FVector(WorldIndex.X + i * Seed, WorldIndex.Y + i * Seed, WorldIndex.Z) * 0.1f);
				if(Noise >= SpawnRate && (1.f - FMathHelper::HashRand(WorldIndex.ToVector2D(), Seed)) <= Iter.SpawnRate)
				{
					GenerateOre(WorldIndex, Iter);
					break;
				}
			)
		}
	)
}

void UVoxelOreGenerator::GenerateOre(FIndex InIndex, const FVoxelOreGenerateData& InGenerateData)
{
	const int Size = FMathHelper::HashRandRange(InIndex.ToVector2D(), InGenerateData.MinSize, InGenerateData.MaxSize, Seed);
	for(int i = 0; i < Size; i++)
	{
		const FIndex Offset = FIndex(
			FMathHelper::HashRandRange(InIndex.ToVector2D(), -1, 1, Seed),
			FMathHelper::HashRandRange(InIndex.ToVector2D(), -1, 1, Seed),
			FMathHelper::HashRandRange(InIndex.ToVector2D(), -1, 1, Seed)
		);
		if(!Module->HasVoxelByIndex(InIndex + Offset))
		{
			Module->SetVoxelByIndex(InIndex + Offset, InGenerateData.VoxelType);
		}
	}
}
