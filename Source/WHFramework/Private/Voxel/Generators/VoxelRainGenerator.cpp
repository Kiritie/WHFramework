// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelRainGenerator.h"

#include "Math/MathHelper.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelRainGenerator::UVoxelRainGenerator()
{
	Seed = 2183;
	SpawnRate = 0.98f;
	MaxDistance = 40.f;
	
	_Waters = TSet<FIndex>();
}

void UVoxelRainGenerator::Generate(AVoxelChunk* InChunk)
{
	FScopeLock ScopeLock(&CriticalSection);

	const float Possible = FMathHelper::HashRand(InChunk->GetIndex().ToVector2D() + FVector2D(13.51f, 2.16f), Seed);
	if(Possible <= SpawnRate) return;

	_Waters.Reset();

	const int32 X = FMathHelper::HashRandInt(InChunk->GetIndex().ToVector2D(), Seed) % (int32)(Module->GetWorldData().ChunkSize.X - 1) + 1;
	const int32 Y = FMathHelper::HashRandInt(InChunk->GetIndex().ToVector2D() + FVector2D(-1.512f, 41.421f), Seed) %(int32)(Module->GetWorldData().ChunkSize.Y - 1) + 1;
	const int32 Z = InChunk->GetTopography(FIndex(X, Y)).Height + 1;
	if(Module->HasVoxelByIndex(FIndex(X, Y, Z), true))
	{
		Flow(InChunk, MaxDistance, X, Y, Z);
	}
}

void UVoxelRainGenerator::Flow(AVoxelChunk* InChunk, float InRain, int InX, int InY, int InZ)
{
	if(InRain < 0.5f) return;

	FIndex Index = FIndex(InChunk->GetWorldIndex().X + InX, InChunk->GetWorldIndex().Y + InY, InZ);
	_Waters.Emplace(Index);

	Index = FIndex(InChunk->GetWorldIndex().X + InX, InChunk->GetWorldIndex().Y + InY, InZ - 1);
	
	if(_Waters.Find(Index)) return;

	if(!Module->HasVoxelByIndex(Index, true))
	{
		Flow(InChunk, InRain - 0.3f, InX, InY, InZ - 1);
		return;
	}

	const int32 Dx[4] = {1, -1, 0, 0};
	const int32 Dy[4] = {0, 0, 1, -1};

	for(int n = 0; n < 4; ++n)
	{
		int32 X = InX + Dx[n];
		int32 Y = InY + Dy[n];

		Index = FIndex(InChunk->GetWorldIndex().X + X, InChunk->GetWorldIndex().Y + Y, InZ);

		if(_Waters.Find(Index)) continue;

		if(!Module->HasVoxelByIndex(Index, true))
		{
			Index = FIndex(InChunk->GetWorldIndex().X + X, InChunk->GetWorldIndex().Y + Y, InZ - 1);
			
			if(_Waters.Find(Index)) continue;

			if(!Module->HasVoxelByIndex(Index, true))
			{
				Flow(InChunk, InRain - 1.0f, X, Y, InZ - 1);
			}
			else
			{
				Flow(InChunk, InRain - 1.0f, X, Y, InZ);
			}
		}
	}

	for(auto& Iter : _Waters)
	{
		Module->SetVoxelByIndex(FIndex(Iter.X, Iter.Y, Iter.Z - 1), EVoxelType::Water);
	}
}
