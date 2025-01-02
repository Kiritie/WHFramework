// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelRainGenerator.h"

#include "Math/MathStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelRainGenerator::UVoxelRainGenerator()
{
	Seed = 2183;
	MaxNum = 3;
	SpawnRate = 0.9f;
	MaxDepth = 80.f;
	Waters = TSet<FIndex>();
}

void UVoxelRainGenerator::Generate(AVoxelChunk* InChunk)
{
	Waters.Reset();

	//初始化雨滴（随机生成雨滴）
	for (int n = 0; n < MaxNum; n++)
	{
		const float Possible = UMathStatics::Rand(InChunk->GetIndex().ToVector2D() + FVector2D(13.51f, 2.16f), Seed);
		if (Possible <= SpawnRate) continue;
		const int32 X = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D(), Seed) % (int32)(Module->GetWorldData().ChunkSize.X - 1) + 1;
		const int32 Y = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D() + FVector2D(-1.512f, 41.421f), Seed) % (int32)(Module->GetWorldData().ChunkSize.Y - 1) + 1;
		const int32 Z = InChunk->GetTopography(FIndex(X, Y)).Height + 1;
		if (Module->HasVoxelByIndex(FIndex(X, Y, Z)))
		{
			Flow(InChunk, MaxDepth, X, Y, Z);
		}
	}
}

void UVoxelRainGenerator::Flow(AVoxelChunk* InChunk, float InRain, int InX, int InY, int InZ)
{
	if (InRain < 0.5f || InZ <= Module->GetWorldData().SeaLevel) return;

	FIndex Index = FIndex(InChunk->GetWorldIndex().X + InX, InChunk->GetWorldIndex().Y + InY, InZ);
	Waters.Emplace(Index);

	//垂流：检测下方踏空
	Index = FIndex(InChunk->GetWorldIndex().X + InX, InChunk->GetWorldIndex().Y + InY, InZ - 1);
	if (Waters.Find(Index)) return;
	if (!Module->HasVoxelByIndex(Index))
	{
		Flow(InChunk, InRain - 0.3f, InX, InY, InZ - 1);
		return;
	}

	const int32 Dx[4] = {1, -1, 0, 0};
	const int32 Dy[4] = {0, 0, 1, -1};

	for (int n = 0; n < 4; ++n)
	{
		int32 X = InX + Dx[n];
		int32 Y = InY + Dy[n];

		Index = FIndex(InChunk->GetWorldIndex().X + X, InChunk->GetWorldIndex().Y + Y, InZ);

		if (Waters.Find(Index)) continue;

		//水平流向无障碍
		if (!Module->HasVoxelByIndex(Index))
		{
			Index = FIndex(InChunk->GetWorldIndex().X + X, InChunk->GetWorldIndex().Y + Y, InZ - 1);
			if (Waters.Find(Index)) continue;

			if (!Module->HasVoxelByIndex(Index))
			{
				Flow(InChunk, InRain - 1.0f, X, Y, InZ - 1);
			}
			else
			{
				Flow(InChunk, InRain - 1.0f, X, Y, InZ);
			}
		}
	}

	for (auto& Iter : Waters)
	{
		Module->SetVoxelByIndex(FIndex(Iter.X, Iter.Y, Iter.Z - 1), EVoxelType::Water);
	}
}
