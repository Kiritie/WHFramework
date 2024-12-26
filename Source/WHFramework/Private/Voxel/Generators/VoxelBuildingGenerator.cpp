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
	Domains = {};
	Roads = {};
	StartPoint = FVector2D::ZeroVector;
	BuildingPos = {};
}

void UVoxelBuildingGenerator::Generate(AVoxelChunk* InChunk)
{
	if (UMathStatics::Rand(InChunk->GetIndex().ToVector2D(), Seed) < 0.98f)return;

	//Domains.Reset();

	DevelopeDomains(InChunk);
	PlaceBuildings(InChunk);
	PlacePaths(InChunk);
}

void UVoxelBuildingGenerator::DevelopeDomains(AVoxelChunk* InChunk)
{
	StartPoint = FVector2D(InChunk->GetIndex().X * 16 + 7, InChunk->GetIndex().Y * 16 + 7);

	std::priority_queue<
		std::pair<float, FVector2D>,
		std::vector<std::pair<float, FVector2D>>,
		std::greater<std::pair<float, FVector2D>>
	> s;
	s.emplace(0, StartPoint);

	int32 count = 0;
	const int32 dx[9] = {1, -1, 0, 0, 1, -1, 1, -1, 0};
	const int32 dy[9] = {0, 0, 1, -1, 1, -1, -1, 1, 0};

	UE_LOG(LogTemp, Warning, TEXT("develop begin"));

	while (!s.empty())
	{
		count++;
		float cost = s.top().first;
		FVector2D p = s.top().second;
		s.pop();

		for (int d = 0; d < 9; ++d)
			Domains.Emplace(UMathStatics::Index(p.X + dx[d], p.Y + dy[d]));

		//设置最高发展度费用
		if (cost > 7)break;

		int32 centerHeight = Module->GetTopographyByIndex(FIndex(p.X, p.Y)).Height;

		const int32 dx1[4] = {1, -1, 0, 0};
		const int32 dy1[4] = {0, 0, 1, -1};

		for (int d = 0; d < 4; ++d)
		{
			int32 x = p.X + dx1[d] * 3;
			int32 y = p.Y + dy1[d] * 3;

			if (Domains.Find(UMathStatics::Index(x, y)))continue;

			int32 height = Module->GetTopographyByIndex(FIndex(x, y)).Height;
			if (height <= Module->GetWorldData().SeaLevel || height > 1000) { continue; }

			int32 deltaheight = FMath::Abs(centerHeight - height);

			s.emplace(cost + 0.5f + deltaheight * 0.5f, FVector2D(x, y));
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("devlop %d"), count);
}

void UVoxelBuildingGenerator::PlaceBuildings(AVoxelChunk* InChunk)
{
	//[第几个建筑][长、宽]
	const int32 buildingSize[3][2] = {{10, 6}, {8, 6}, {6, 6}};
	const int32 dx[4] = {1, -1, 0, 0};
	const int32 dy[4] = {0, 0, 1, -1};

	int32 count = 0;

	std::queue<FVector2D> q;
	q.push(StartPoint);

	while (!q.empty())
	{
		++count;

		auto pos = q.front();
		q.pop();

		int32 index = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D() + FVector2D(count, -count) * 107, Seed) % 3;
		int32 rotate = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D() + FVector2D(count, -count) * 17, Seed) % 4;

		bool test = PlaceOneBuilding(InChunk, pos.X, pos.Y, index, rotate) || PlaceOneBuilding(InChunk, pos.X, pos.Y, index, (rotate + 1) % 4);
		if (!test)
		{
			continue;
		}

		int32 offset = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D() + FVector2D(count, -count) * 67, Seed) % 3 + 5;
		int32 offsetX = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D() + FVector2D(count, count) * 61, Seed) % 5 - 2;
		int32 offsetY = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D() + FVector2D(-count, count) * 117, Seed) % 5 - 2;

		for (int i = 0; i < 4; ++i)
		{
			q.push(FVector2D
				(pos.X + dx[i] * (offset + buildingSize[index][0]) + offsetX,
				 pos.Y + dy[i] * (offset + buildingSize[index][1]) + offsetY));
		}
	}
}

bool UVoxelBuildingGenerator::PlaceOneBuilding(AVoxelChunk* InChunk, int32 InX, int32 InY, int32 index, int32 InRotate)
{
	//[第几个建筑][长、宽]
	const int32 buildingSize[3][2] = {{10, 6}, {8, 6}, {6, 6}};

	int rotateIndex = InRotate % 2;
	int updown = buildingSize[index][rotateIndex] / 2;
	int leftright = buildingSize[index][!rotateIndex] / 2;

	float aver = 0;
	for (int i = -updown; i < updown; ++i)
		for (int j = -leftright; j < leftright; ++j)
		{
			//若不在发展域，则无需生成建筑         
			if (!Domains.Find(UMathStatics::Index(InX + i, InY + j))) { return false; }

			//若地面被挖空，则无需生成建筑
			FVector pos3D = FVector(InX + i, InY + j, Module->GetTopographyByIndex(FIndex(InX + i, InY + j)).Height);
			if (Module->HasVoxelByIndex(pos3D)) { return false; }

			int32 h = Module->GetTopographyByIndex(FIndex(InX + i, InY + j)).Height;
			aver += h;
		}

	aver /= (buildingSize[index][0] * buildingSize[index][1]);
	aver = floor(aver + 0.5f);

	//低于海平面，没必要生成房屋
	if (aver <= Module->GetWorldData().SeaLevel)return false;

	for (int i = -updown; i < updown; ++i)
		for (int j = -leftright; j < leftright; ++j)
		{
			Module->GetTopographyByIndex(FIndex(InX + i, InY + j)).Height = aver;
			Domains.Remove(UMathStatics::Index(InX + i, InY + j));
		}

	//地表插入空气，以免生成树木花草
	for (int i = -updown - 1; i < updown + 1; ++i)
		for (int j = -leftright - 1; j < leftright + 1; ++j)
		{
			FVector pos = FVector(InX + i, InY + j, Module->GetTopographyByIndex(FIndex(InX + i, InY + j)).Height + 1);
			Module->SetVoxelByIndex(pos, FVoxelItem::Empty);
		}

	FVoxelBuildingSaveData BuildingData;
	BuildingData.ID = index + 1;
	BuildingData.Location = FVector(InX, InY, aver + 1);
	BuildingData.Angle = InRotate;
	
	InChunk->AddBuilding(BuildingData);
	
	Domains.Emplace(UMathStatics::Index(InX - updown, InY - leftright));
	BuildingPos.Push(FVector2D(InX - updown, InY - leftright));
	return true;
}

void UVoxelBuildingGenerator::PlacePaths(AVoxelChunk* InChunk)
{
	FPathFinder::SetConditionInBarrier([this](FVector2D pos) { return InBarrier(pos); });
	FPathFinder::SetWeightFormula([this](FVector2D pos, FVector2D endPos, float cost) { return WeightFormula(pos, endPos, cost); });
	int n = BuildingPos.Num();
	//两两寻路，进行道路连接
	for (int i = 0; i < n; ++i)
		for (int j = i + 1; j < n; ++j)
		{
			auto path = FPathFinder::FindPath(BuildingPos[i], BuildingPos[j]);

			UE_LOG(LogTemp, Warning, TEXT("path roads num = %d"), path.Num());

			for (FVector2D pos : path)
			{
				Roads.Emplace(UMathStatics::Index(pos.X, pos.Y));
				FVector pos3D = FVector(pos.X, pos.Y, Module->GetTopographyByIndex(FIndex(pos.X, pos.Y)).Height);
				//被挖空则不生成
				if (Module->HasVoxelByIndex(pos3D))
					continue;

				Module->SetVoxelByIndex(FIndex(pos.X, pos.Y, Module->GetTopographyByIndex(FIndex(pos.X, pos.Y)).Height), EVoxelType::Cobble_Stone);
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

	FVector2D dist = (InEndLocation - InStartLocation).GetAbs();
	float predict = (dist.X + dist.Y) * 1.41f - FMath::Max(dist.X, dist.Y) * 0.41f + InCost;

	return TPair<float, float>(InCost, predict);
}
