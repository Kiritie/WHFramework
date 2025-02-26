// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelHoleGenerator.h"

#include "Math/MathHelper.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Generators/VoxelCaveGenerator.h"

UVoxelHoleGenerator::UVoxelHoleGenerator()
{
	Seed = 394835;
	CrystalSize = 2.f;
	SpawnRate = 0.7f;
	MinRadius = 2.f;
	MaxRadius = 3.f;
}

void UVoxelHoleGenerator::Generate(AVoxelChunk* InChunk)
{
	GenerateHole(InChunk->GetWorldIndex());
}

void UVoxelHoleGenerator::GenerateHole(FIndex InIndex) const
{
	if(FMathHelper::HashRand(InIndex.ToVector2D(), Seed) < SpawnRate) return;

	const FIndex SurfaceIndex = FIndex(InIndex.X, InIndex.Y, Module->GetTopographyByIndex(InIndex).Height);
	if(SurfaceIndex.Z == 0) return;

	const FIndex Hole = FindNearestHoleIndexMultiDirection(SurfaceIndex, 100);
	if(Hole == FIndex::ZeroIndex) return;

	const int32 Radius = FMathHelper::HashRandRange(InIndex.ToVector2D(), MinRadius, MaxRadius);
	const TArray<FIndex> Path = GenerateHoleSlopePath(SurfaceIndex, Hole);
	for(auto& Iter : Path)
	{
		for(int32 X = -Radius; X <= Radius; X++)
		{
			for(int32 Y = -Radius; Y <= Radius; Y++)
			{
				for(int32 Z = -Radius; Z <= Radius; Z++)
				{
					const FVector Offset(X, Y, Z);
					if(Offset.Size() <= Radius)
					{
						Module->SetVoxelByIndex(Iter + Offset, FVoxelItem::Empty);
					}
				}
			}
		}
	}
}

FIndex UVoxelHoleGenerator::FindNearestHoleIndex(FIndex InStartIndex, FVector InDirection, int32 InMaxSearchDistance) const
{
	const FVector NormalizedDir = InDirection.GetSafeNormal();

	for(int32 Step = 0; Step < InMaxSearchDistance; Step++)
	{
		const FIndex CurrentIndex(
			InStartIndex.X + FMath::RoundToInt(NormalizedDir.X * Step),
			InStartIndex.Y + FMath::RoundToInt(NormalizedDir.Y * Step),
			InStartIndex.Z + FMath::RoundToInt(NormalizedDir.Z * Step)
		);

		if(Module->GetVoxelGenerator<UVoxelCaveGenerator>()->IsCave(CurrentIndex))
		{
			return CurrentIndex;
		}
	}

	return FIndex::ZeroIndex;
}

FIndex UVoxelHoleGenerator::FindNearestHoleIndexMultiDirection(FIndex InStartIndex, int32 InMaxSearchDistance) const
{
	const TArray<FVector> Directions = {
		FVector(0, 0, -1),
		FVector(1, 0, -1),
		FVector(-1, 0, -1),
		FVector(0, 1, -1),
		FVector(0, -1, -1)
	};

	FIndex NearestIndex = FIndex::ZeroIndex;
	int32 MinDistance = INT_MAX;

	for(auto& Iter : Directions)
	{
		const FIndex HoleIndex = FindNearestHoleIndex(InStartIndex, Iter, InMaxSearchDistance);
		if(HoleIndex != FIndex::ZeroIndex)
		{
			const int32 Distance = FIndex::Distance(InStartIndex, HoleIndex);
			if(Distance < MinDistance)
			{
				MinDistance = Distance;
				NearestIndex = HoleIndex;
			}
		}
	}

	return NearestIndex;
}

TArray<FIndex> UVoxelHoleGenerator::GenerateHoleSlopePath(FIndex InStartIndex, FIndex InEndIndex) const
{
	TArray<FIndex> Path;
	
	FIndex CurrentIndex = InStartIndex;
	const FVector Direction = (InEndIndex - InStartIndex).ToVector().GetSafeNormal();
	
	while(CurrentIndex != InEndIndex)
	{
		Path.Add(CurrentIndex);
		CurrentIndex.X += FMath::RoundToInt(Direction.X);
		CurrentIndex.Y += FMath::RoundToInt(Direction.Y);
		CurrentIndex.Z += FMath::RoundToInt(Direction.Z);
	}
	Path.Add(InEndIndex);
	
	return Path;
}
