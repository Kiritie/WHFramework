// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelLakeGenerator.h"

#include "Math/MathStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"

UVoxelLakeGenerator::UVoxelLakeGenerator()
{
	Seed = 8997632;
	SpawnRate = 0.9f;
	MinDistance = 80;
	MinRadius = 5.f;
	MaxRadius = 30.f;
	MinDepth = 2.f;
	MaxDepth = 5.f;
}

void UVoxelLakeGenerator::Generate(AVoxelChunk* InChunk)
{
	GenerateLake(InChunk->GetWorldIndex());
}

void UVoxelLakeGenerator::GenerateLake(FIndex InIndex)
{
	if (UMathStatics::Rand(InIndex.ToVector2D(), Seed) < SpawnRate) return;

	if (InIndex.X % MinDistance != 0 || InIndex.Y % MinDistance != 0) return;

	const float LakeRadius = UMathStatics::RandRange(InIndex.ToVector2D(), 5.0f, 30.0f, Seed);
	const float LakeDepth = UMathStatics::RandRange(InIndex.ToVector2D(), 3.0f, 5.0f, Seed);

	const FIndex LakeCenter = FIndex(InIndex.X, InIndex.Y, Module->GetTopographyByIndex(InIndex).Height - LakeRadius / 7.f);

	GenerateLakeDepression(LakeCenter, LakeRadius, LakeDepth);
	SmoothLakeEdges(LakeCenter, LakeRadius, 3);
	FillLakeWater(LakeCenter, LakeRadius);
}

void UVoxelLakeGenerator::GenerateLakeDepression(FIndex InIndex, float InRadius, float InDepth)
{
	for (int32 X = -InRadius; X <= InRadius; X++)
	{
		for (int32 Y = -InRadius; Y <= InRadius; Y++)
		{
			FIndex Pos = InIndex + FIndex(X, Y, 0);
			float Distance = FVector2D(X, Y).Size();
			if (Distance <= InRadius)
			{
				float Depression = InDepth * FMath::Cos((Distance / InRadius) * PI / 2);
				FVoxelTopography Topography = Module->GetTopographyByIndex(Pos);
				Topography.Height = InIndex.Z - Depression;
				Module->SetTopographyByIndex(Pos, Topography);
			}
		}
	}
}

void UVoxelLakeGenerator::SmoothLakeEdges(FIndex InIndex, float InRadius, int32 InIterations)
{
	for (int32 Iteration = 0; Iteration < InIterations; Iteration++)
	{
		for (int32 X = -InRadius; X <= InRadius; X++)
		{
			for (int32 Y = -InRadius; Y <= InRadius; Y++)
			{
				FIndex Pos = InIndex + FIndex(X, Y, 0);
				float Distance = FVector2D(X, Y).Size();
				if (Distance > InRadius - 1 && Distance <= InRadius + 1)
				{
					float AvgHeight = 0;
					int32 Count = 0;
					for (int32 DX = -1; DX <= 1; DX++)
					{
						for (int32 DY = -1; DY <= 1; DY++)
						{
							FIndex NeighborPos = Pos + FIndex(DX, DY, 0);
							AvgHeight += Module->GetTopographyByIndex(NeighborPos).Height;
							Count++;
						}
					}
					AvgHeight /= Count;
					FVoxelTopography Topography = Module->GetTopographyByIndex(Pos);
					Topography.Height = AvgHeight;
					Module->SetTopographyByIndex(Pos, Topography);
				}
			}
		}
	}
}

void UVoxelLakeGenerator::FillLakeWater(FIndex InIndex, float InRadius)
{
	for (int32 X = -InRadius; X <= InRadius; X++)
	{
		for (int32 Y = -InRadius; Y <= InRadius; Y++)
		{
			FIndex Pos = InIndex + FIndex(X, Y, 0);
			int32 LakeBedHeight = Module->GetTopographyByIndex(FIndex(Pos.X, Pos.Y)).Height;
			for (int32 Z = LakeBedHeight; Z < InIndex.Z; Z++)
			{
				FIndex WaterPos(Pos.X, Pos.Y, Z);
				Module->SetVoxelByIndex(WaterPos, EVoxelType::Water);
			}
		}
	}
}
