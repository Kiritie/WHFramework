// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelLakeGenerator.h"

#include "Math/MathHelper.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"

UVoxelLakeGenerator::UVoxelLakeGenerator()
{
	Seed = 8997632;
	SpawnRate = 0.8f;
	NoiseScale = 0.05f;
	MinDistance = 8;
	MinRadius = 4.f;
	MaxRadius = 30.f;
	MinDepth = 3.f;
	MaxDepth = 5.f;
}

void UVoxelLakeGenerator::Generate(AVoxelChunk* InChunk)
{
	if(InChunk->GetIndex().X % MinDistance != 0 || InChunk->GetIndex().Y % MinDistance != 0) return;

	GenerateLake(InChunk->GetWorldIndex());
}

void UVoxelLakeGenerator::GenerateLake(FIndex InIndex)
{
	if(FMathHelper::HashRand(InIndex.ToVector2D(), Seed) < SpawnRate) return;

	const float LakeRadius = FMathHelper::HashRandRange(InIndex.ToVector2D(), MinRadius, MaxRadius, Seed);
	const float LakeDepth = FMathHelper::HashRandRange(InIndex.ToVector2D(), MinDepth, MaxDepth, Seed);

	const FIndex LakeCenter = FIndex(InIndex.X, InIndex.Y, GetSurfaceHeight(InIndex, LakeRadius));
	if(LakeCenter.Z == 0) return;

	GenerateLakeDepression(LakeCenter, LakeRadius, LakeDepth);
	SmoothLakeEdges(LakeCenter, LakeRadius, 3);
	FillLakeWater(LakeCenter, LakeRadius);
}

void UVoxelLakeGenerator::GenerateLakeDepression(FIndex InIndex, float InRadius, float InDepth)
{
	for(int32 X = -InRadius; X <= InRadius; X++)
	{
		for(int32 Y = -InRadius; Y <= InRadius; Y++)
		{
			const FIndex Index = InIndex + FIndex(X, Y, 0);
			const float Distance = FVector2D(X, Y).Size();
			const float EffectiveRadius = GetEffectiveRadius(Index, InRadius);
			if(Distance <= EffectiveRadius)
			{
				const float Depression = InDepth * FMath::Cos(Distance / EffectiveRadius * PI / 2);
				FVoxelTopography Topography = Module->GetTopographyByIndex(Index);
				Topography.Height = InIndex.Z - Depression;
				Module->SetTopographyByIndex(Index, Topography);
			}
		}
	}
}

void UVoxelLakeGenerator::SmoothLakeEdges(FIndex InIndex, float InRadius, int32 InIterations)
{
	DON(InIterations,
		for(int32 X = -InRadius; X <= InRadius; X++)
		{
			for(int32 Y = -InRadius; Y <= InRadius; Y++)
			{
				const FIndex Index = InIndex + FIndex(X, Y, 0);
				const float Distance = FVector2D(X, Y).Size();
				const float EffectiveRadius = GetEffectiveRadius(Index, InRadius);
				if(Distance > EffectiveRadius - 1 && Distance <= EffectiveRadius + 1)
				{
					float AvgHeight = 0;
					int32 Count = 0;
					for(int32 DX = -1; DX <= 1; DX++)
					{
						for(int32 DY = -1; DY <= 1; DY++)
						{
							const FIndex NeighborIndex = Index + FIndex(DX, DY, 0);
							AvgHeight += Module->GetTopographyByIndex(NeighborIndex).Height;
							Count++;
						}
					}
					AvgHeight /= Count;
					FVoxelTopography Topography = Module->GetTopographyByIndex(Index);
					Topography.Height = AvgHeight;
					Module->SetTopographyByIndex(Index, Topography);
				}
			}
		}
	)
}

void UVoxelLakeGenerator::FillLakeWater(FIndex InIndex, float InRadius)
{
	for(int32 X = -InRadius; X <= InRadius; X++)
	{
		for(int32 Y = -InRadius; Y <= InRadius; Y++)
		{
			const FIndex Index = InIndex + FIndex(X, Y, 0);
			const int32 LakeBedHeight = Module->GetTopographyByIndex(Index).Height;
			for(int32 Z = LakeBedHeight; Z < InIndex.Z + 1; Z++)
			{
				const FIndex WaterIndex(Index.X, Index.Y, Z);
				Module->SetVoxelByIndex(WaterIndex, EVoxelType::Water);
			}
		}
	}
}

int32 UVoxelLakeGenerator::GetSurfaceHeight(FIndex InIndex, float InRadius) const
{
	int32 SurfaceHeight = Module->GetTopographyByIndex(InIndex).Height;
	for(int32 X = -InRadius; X <= InRadius; X++)
	{
		for(int32 Y = -InRadius; Y <= InRadius; Y++)
		{
			const FIndex Index = InIndex + FIndex(X, Y, 0);
			SurfaceHeight = FMath::Min(Module->GetTopographyByIndex(Index).Height, SurfaceHeight);
		}
	}
	return SurfaceHeight;
}

int32 UVoxelLakeGenerator::GetEffectiveRadius(FIndex InIndex, float InRadius) const
{
	return InRadius *(1.0f + Module->GetVoxelNoise2D(FVector2D(InIndex.X * NoiseScale, InIndex.Y * NoiseScale)) * 0.5f);
}
