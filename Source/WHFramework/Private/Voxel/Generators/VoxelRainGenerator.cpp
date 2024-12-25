// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelRainGenerator.h"

#include "Math/MathStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelRainGenerator::UVoxelRainGenerator()
{
	Waters = TSet<uint64>();
}

void UVoxelRainGenerator::Generate(AVoxelChunk* InChunk)
{
	Waters.Reset();

	//初始化雨滴（随机生成雨滴）
	for (int n = 1; n > 0; --n)
	{
		float possible = UMathStatics::Rand(InChunk->GetIndex().ToVector2D() + FVector2D(13.51f, 2.16f), Module->GetWorldData().WorldSeed);
		if (possible <= 0.95f) continue;
		int32 x = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D(), Module->GetWorldData().WorldSeed) % 15 + 1;
		int32 y = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D() + FVector2D(-1.512f, 41.421f), Module->GetWorldData().WorldSeed) % 15 + 1;
		Flow(InChunk, 60.0f, x, y, InChunk->GetTopography(FIndex(x, y)).Height + 1);
	}
}

void UVoxelRainGenerator::Flow(AVoxelChunk* InChunk, float InRain, int InX, int InY, int InZ)
{
	if (InRain < 0.5f/* || k<=SeaLevel*/)return;

	uint64 index = UMathStatics::Index(InChunk->GetIndex().X * 16 + InX, InChunk->GetIndex().Y * 16 + InY, InZ);
	Waters.Emplace(index);

	//垂流：检测下方踏空
	FVector pos = FVector(InChunk->GetIndex().X * 16 + InX, InChunk->GetIndex().Y * 16 + InY, InZ - 1);
	index = UMathStatics::Index(pos.X, pos.Y, pos.Z);
	if (Waters.Find(index)) { return; }
	if (!Module->HasVoxelByIndex(pos))
	{
		Flow(InChunk, InRain - 0.3f, InX, InY, InZ - 1);
		return;
	}

	const int32 dx[4] = {1, -1, 0, 0};
	const int32 dy[4] = {0, 0, 1, -1};

	for (int d = 0; d < 4; ++d)
	{
		int32 x = InX + dx[d];
		int32 y = InY + dy[d];

		pos = FVector(InChunk->GetIndex().X * 16 + x, InChunk->GetIndex().Y * 16 + y, InZ);
		index = UMathStatics::Index(pos.X, pos.Y, pos.Z);

		if (Waters.Find(index))continue;

		//水平流向无障碍
		if (!Module->HasVoxelByIndex(pos))
		{
			pos = FVector(InChunk->GetIndex().X * 16 + x, InChunk->GetIndex().Y * 16 + y, InZ - 1);
			index = UMathStatics::Index(pos.X, pos.Y, pos.Z);
			if (Waters.Find(index)) { continue; }

			if (!Module->HasVoxelByIndex(pos))
			{
				Flow(InChunk, InRain - 1.0f, x, y, InZ - 1);
			}
			else
			{
				Flow(InChunk, InRain - 1.0f, x, y, InZ);
			}
		}
	}

	for (uint64 index1 : Waters)
	{
		auto vec = UMathStatics::UnIndex(index1);
		Module->SetVoxelByIndex(FVector(vec.X, vec.Y, vec.Z - 1), EVoxelType::Water);
	}
}
