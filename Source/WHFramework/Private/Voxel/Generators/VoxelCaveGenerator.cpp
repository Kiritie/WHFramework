// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelCaveGenerator.h"

#include "Math/MathStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelCaveGenerator::UVoxelCaveGenerator()
{
	Seed = 394835;
	CrystalSize = 2.f;
	Times = 3;
	MaxDepth = 5;
	SpawnEntryRate = 0.7f;
	MinEntryRadius = 2.f;
	MaxEntryRadius = 3.f;
}

void UVoxelCaveGenerator::Generate(AVoxelChunk* InChunk)
{
	ITER_INDEX2D(Index, Module->GetWorldData().ChunkSize, false,
		const int32 Height = InChunk->GetTopography(Index).Height - 10;
		for(int Z = FMath::Max(Height - MaxDepth, 0); Z <= Height; Z++)
		{
			const FIndex WorldIndex = InChunk->LocalIndexToWorld(FIndex(Index.X, Index.Y, Z));
			//若高于一定阈值，挖空
			if (IsCave(WorldIndex))
			{
				Module->SetVoxelByIndex(WorldIndex, FVoxelItem::Empty);
			}
		}
	)
	GenerateCaveEntry(InChunk->GetWorldIndex());
}

bool UVoxelCaveGenerator::IsCave(FIndex InIndex) const
{
	if(InIndex.Z <= 0) return false;
	
	// 分形噪声参数
	const float Scale = 0.05f;
	const float Persistence = 0.5f;

	float TotalNoise = 0.f;
	float Frequency = Scale;
	float Amplitude = 1.f;

	// 叠加多层噪声
	for (int i = 0; i < Times; i++)
	{
		TotalNoise += Module->GetVoxelNoise3D(FVector(InIndex.X * Frequency, InIndex.Y * Frequency, InIndex.Z * Frequency)) * Amplitude;

		Frequency *= 2.f;
		Amplitude *= Persistence;
	}

	// 噪声值 > 0.3 时为洞穴
	return TotalNoise > 0.3f;
}

// 生成洞穴入口
void UVoxelCaveGenerator::GenerateCaveEntry(FIndex InIndex) const
{
	if(UMathStatics::Rand(InIndex.ToVector2D(), Seed) < SpawnEntryRate) return;

	const int32 Height = Module->GetTopographyByIndex(InIndex).Height;
	if(Height == 0) return;

	const FIndex SurfacePos = FIndex(InIndex.X, InIndex.Y, Height);

	const FIndex CaveEntry = FindNearestCaveEntryIndexMultiDirection(SurfacePos, 100);
	if (CaveEntry == FIndex::ZeroIndex) return;

	const int32 EntryRadius = UMathStatics::RandRange(InIndex.ToVector2D(), MinEntryRadius, MaxEntryRadius);
	for(auto& Pos : GenerateCaveEntrySlopePath(SurfacePos, CaveEntry))
	{
		for (int32 X = -EntryRadius; X <= EntryRadius; X++)
		{
			for (int32 Y = -EntryRadius; Y <= EntryRadius; Y++)
			{
				for (int32 Z = -EntryRadius; Z <= EntryRadius; Z++)
				{
					const FVector Offset(X, Y, Z);
					if (Offset.Size() <= EntryRadius)
					{
						Module->SetVoxelByIndex(Pos + Offset, FVoxelItem::Empty);
					}
				}
			}
		}
	}
}

FIndex UVoxelCaveGenerator::FindNearestCaveEntryIndex(const FIndex& InStartIndex, const FVector& InDirection, int32 InMaxSearchDistance) const
{
	const FVector NormalizedDir = InDirection.GetSafeNormal();

	for (int32 Step = 0; Step < InMaxSearchDistance; Step++)
	{
		FIndex CurrentPos(
			InStartIndex.X + FMath::RoundToInt(NormalizedDir.X * Step),
			CurrentPos.Y = InStartIndex.Y + FMath::RoundToInt(NormalizedDir.Y * Step),
			CurrentPos.Z = InStartIndex.Z + FMath::RoundToInt(NormalizedDir.Z * Step)
		);

		if (IsCave(CurrentPos))
		{
			return CurrentPos;
		}
	}

	return FIndex::ZeroIndex;
}

FIndex UVoxelCaveGenerator::FindNearestCaveEntryIndexMultiDirection(const FIndex& InStartIndex, int32 InMaxSearchDistance) const
{
	TArray<FVector> Directions = {
		FVector(0, 0, -1),
		FVector(1, 0, -1),
		FVector(-1, 0, -1),
		FVector(0, 1, -1),
		FVector(0, -1, -1)
	};

	FIndex NearestIndex = FIndex::ZeroIndex;
	int32 MinDistance = INT_MAX;

	for (const FVector& Dir : Directions)
	{
		const FIndex CavePos = FindNearestCaveEntryIndex(InStartIndex, Dir, InMaxSearchDistance);
		if (CavePos != FIndex::ZeroIndex)
		{
			const int32 Distance = FIndex::Distance(InStartIndex, CavePos);
			if (Distance < MinDistance)
			{
				MinDistance = Distance;
				NearestIndex = CavePos;
			}
		}
	}

	return NearestIndex;
}

TArray<FIndex> UVoxelCaveGenerator::GenerateCaveEntrySlopePath(FIndex InStartIndex, FIndex InEndIndex) const
{
	TArray<FIndex> Path;
	
	FIndex CurrentIndex = InStartIndex;
	FVector Direction = (InEndIndex - InStartIndex).ToVector().GetSafeNormal();
	
	while (CurrentIndex != InEndIndex)
	{
		Path.Add(CurrentIndex);
		CurrentIndex.X += FMath::RoundToInt(Direction.X);
		CurrentIndex.Y += FMath::RoundToInt(Direction.Y);
		CurrentIndex.Z += FMath::RoundToInt(Direction.Z);
	}
	Path.Add(InEndIndex);
	
	return Path;
}
