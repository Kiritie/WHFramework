// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelBuildingGenerator.h"
#include "GameFramework/Actor.h"
#include "Math/MathStatics.h"
#include "Tool/PathFinder.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelBuildingGenerator::UVoxelBuildingGenerator()
{
	Seed = 453;
	SpawnRate = 0.985f;

	Domains = {};
	Roads = {};
	StartPoint = FVector2D::ZeroVector;
	BuildingPos = {};
}

void UVoxelBuildingGenerator::Generate(AVoxelChunk* InChunk)
{
	if (UMathStatics::Rand(InChunk->GetIndex().ToVector2D(), Seed) < SpawnRate)return;

	//Domains.Reset();

	DevelopeDomains(InChunk);
	PlaceBuildings(InChunk);
	PlacePaths(InChunk);
}

void UVoxelBuildingGenerator::DevelopeDomains(AVoxelChunk* InChunk)
{
	StartPoint = FVector2D(InChunk->GetWorldIndex().X + 7, InChunk->GetWorldIndex().Y + 7);

	std::priority_queue<
		std::pair<float, FVector2D>,
		std::vector<std::pair<float, FVector2D>>,
		std::greater<std::pair<float, FVector2D>>
	> Points;
	Points.emplace(0, StartPoint);

	int32 Count = 0;
	const int32 Dx[9] = {1, -1, 0, 0, 1, -1, 1, -1, 0};
	const int32 Dy[9] = {0, 0, 1, -1, 1, -1, -1, 1, 0};

	// UE_LOG(LogTemp, Warning, TEXT("develop begin"));

	while (!Points.empty())
	{
		Count++;
		float Cost = Points.top().first;
		FVector2D P = Points.top().second;
		Points.pop();

		for (int d = 0; d < 9; ++d)
		{
			Domains.Emplace(UMathStatics::Index(P.X + Dx[d], P.Y + Dy[d]));
		}

		//设置最高发展度费用
		if (Cost > 7) break;

		int32 CenterHeight = Module->GetTopographyByIndex(FIndex(P.X, P.Y)).Height;

		const int32 Dx1[4] = {1, -1, 0, 0};
		const int32 Dy1[4] = {0, 0, 1, -1};

		for (int d = 0; d < 4; ++d)
		{
			int32 x = P.X + Dx1[d] * 3;
			int32 y = P.Y + Dy1[d] * 3;

			if (Domains.Find(UMathStatics::Index(x, y))) continue;

			int32 Height = Module->GetTopographyByIndex(FIndex(x, y)).Height;
			if (Height <= Module->GetWorldData().SeaLevel || Height > 1000) continue;

			int32 DeltaHeight = FMath::Abs(CenterHeight - Height);

			Points.emplace(Cost + 0.5f + DeltaHeight * 0.5f, FVector2D(x, y));
		}
	}

	// UE_LOG(LogTemp, Warning, TEXT("devlop %d"), count);
}

void UVoxelBuildingGenerator::PlaceBuildings(AVoxelChunk* InChunk)
{
	//[第几个建筑][长、宽]
	const int32 BuildingSize[3][2] = {{10, 6}, {8, 6}, {6, 6}};
	const int32 Dx[4] = {1, -1, 0, 0};
	const int32 Dy[4] = {0, 0, 1, -1};

	int32 Count = 0;

	std::queue<FVector2D> Points;
	Points.push(StartPoint);

	while (!Points.empty())
	{
		++Count;

		auto Pos = Points.front();
		Points.pop();

		int32 Index = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D() + FVector2D(Count, -Count) * 107, Seed) % 3;
		int32 Rotate = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D() + FVector2D(Count, -Count) * 17, Seed) % 4;

		if (!PlaceOneBuilding(InChunk, Pos.X, Pos.Y, Index, Rotate) || PlaceOneBuilding(InChunk, Pos.X, Pos.Y, Index, (Rotate + 1) % 4))
		{
			continue;
		}

		int32 Offset = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D() + FVector2D(Count, -Count) * 67, Seed) % 3 + 5;
		int32 OffsetX = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D() + FVector2D(Count, Count) * 61, Seed) % 5 - 2;
		int32 OffsetY = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D() + FVector2D(-Count, Count) * 117, Seed) % 5 - 2;

		for (int i = 0; i < 4; ++i)
		{
			Points.push(FVector2D(Pos.X + Dx[i] * (Offset + BuildingSize[Index][0]) + OffsetX, Pos.Y + Dy[i] * (Offset + BuildingSize[Index][1]) + OffsetY));
		}
	}
}

bool UVoxelBuildingGenerator::PlaceOneBuilding(AVoxelChunk* InChunk, int32 InX, int32 InY, int32 index, int32 InRotate)
{
	//[第几个建筑][长、宽]
	const int32 BuildingSize[3][3] = {{10, 6, 7}, {8, 6, 9}, {6, 6, 15}};

	int RotateIndex = InRotate % 2;
	int FrontBack = BuildingSize[index][RotateIndex] / 2;
	int LeftRight = BuildingSize[index][!RotateIndex] / 2;
	int UpDown = BuildingSize[index][2];

	float Aver = 0;
	for (int i = -FrontBack; i < FrontBack; ++i)
	{
		for (int j = -LeftRight; j < LeftRight; ++j)
		{
			//若不在发展域，则无需生成建筑         
			if (!Domains.Find(UMathStatics::Index(InX + i, InY + j))) return false;

			//若地面被挖空，则无需生成建筑
			FIndex Index = FIndex(InX + i, InY + j, Module->GetTopographyByIndex(FIndex(InX + i, InY + j)).Height);
			if (!Module->HasVoxelByIndex(Index)) return false;

			Aver += Module->GetTopographyByIndex(FIndex(InX + i, InY + j)).Height;
		}
	}

	Aver /= (BuildingSize[index][0] * BuildingSize[index][1]);
	Aver = floor(Aver + 0.5f);

	//低于海平面，没必要生成房屋
	if (Aver <= Module->GetWorldData().SeaLevel)return false;

	for (int i = -FrontBack; i < FrontBack; ++i)
	{
		for (int j = -LeftRight; j < LeftRight; ++j)
		{
			for (int k = Module->GetTopographyByIndex(FIndex(InX + i, InY + j)).Height; k <= Aver; ++k)
			{
				FIndex pos = FIndex(InX + i, InY + j, k);
				Module->SetVoxelByIndex(pos, EVoxelType::Cobble_Stone);
			}
			// Module->GetTopographyByIndex(FIndex(InX + i, InY + j)).Height = aver;
			Domains.Remove(UMathStatics::Index(InX + i, InY + j));
		}
	}

	//地表插入空气，以免生成树木花草
	for (int i = -FrontBack - 1; i < FrontBack + 1; ++i)
	{
		for (int j = -LeftRight - 1; j < LeftRight + 1; ++j)
		{
			for (int k = 0; k < UpDown; ++k)
			{
				FIndex Index = FIndex(InX + i, InY + j, Aver + k + 1);
				Module->SetVoxelByIndex(Index, FVoxelItem::Empty, true);
			}
		}
	}

	FVoxelBuildingSaveData BuildingData;
	BuildingData.ID = index + 1;
	BuildingData.Location = FVector(InX, InY, Aver + 1);
	BuildingData.Angle = InRotate;

	InChunk->AddBuilding(BuildingData);

	Domains.Emplace(UMathStatics::Index(InX - FrontBack, InY - LeftRight));
	BuildingPos.Push(FVector2D(InX - FrontBack, InY - LeftRight));
	return true;
}

void UVoxelBuildingGenerator::PlacePaths(AVoxelChunk* InChunk)
{
	FPathFinder::SetConditionInBarrier([this](FVector2D pos) { return InBarrier(pos); });
	FPathFinder::SetWeightFormula([this](FVector2D pos, FVector2D endPos, float cost) { return WeightFormula(pos, endPos, cost); });
	//两两寻路，进行道路连接
	for (int i = 0; i < BuildingPos.Num(); ++i)
	{
		for (int j = i + 1; j < BuildingPos.Num(); ++j)
		{
			auto Path = FPathFinder::FindPath(BuildingPos[i], BuildingPos[j]);

			// UE_LOG(LogTemp, Warning, TEXT("path roads num = %d"), path.Num());

			for (FVector2D Pos : Path)
			{
				Roads.Emplace(UMathStatics::Index(Pos.X, Pos.Y));
				FIndex Index = FIndex(Pos.X, Pos.Y, Module->GetTopographyByIndex(FIndex(Pos.X, Pos.Y)).Height);
				//被挖空则不生成
				if (!Module->HasVoxelByIndex(Index)) continue;

				Module->SetVoxelByIndex(FIndex(Pos.X, Pos.Y, Module->GetTopographyByIndex(FIndex(Pos.X, Pos.Y)).Height), EVoxelType::Cobble_Stone);
			}
		}
	}
	BuildingPos.Reset();
}

bool UVoxelBuildingGenerator::InBarrier(FVector2D InLocation)
{
	return !Domains.Contains(UMathStatics::Index(InLocation.X, InLocation.Y));
}

TPair<float, float> UVoxelBuildingGenerator::WeightFormula(FVector2D InStartLocation, FVector2D InEndLocation, float InCost)
{
	if (Roads.Contains(UMathStatics::Index(InStartLocation.X, InStartLocation.Y)))
	{
		InCost -= 0.5f;
	}

	FVector2D Dist = (InEndLocation - InStartLocation).GetAbs();
	float Predict = (Dist.X + Dist.Y) * 1.41f - FMath::Max(Dist.X, Dist.Y) * 0.41f + InCost;

	return TPair<float, float>(InCost, Predict);
}
