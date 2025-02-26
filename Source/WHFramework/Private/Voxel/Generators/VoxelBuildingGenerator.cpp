// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelBuildingGenerator.h"
#include "Math/MathStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelBuildingGenerator::UVoxelBuildingGenerator()
{
	Seed = 453;
	SpawnRate = 0.985f;

	PathFinder = FPathFinder();
	PathFinder.SetConditionInBarrier([this](FVector2D Pos) { return InBarrier(Pos); });
	PathFinder.SetWeightFormula([this](FVector2D StartPos, FVector2D EndPos, float Cost) { return WeightFormula(StartPos, EndPos, Cost); });

	_StartPoint = FVector2D::ZeroVector;
	_Domains = {};
	_Roads = {};
	_BuildingPos = {};
}

void UVoxelBuildingGenerator::Generate(AVoxelChunk* InChunk)
{
	float Possible = UMathStatics::Rand(InChunk->GetIndex().ToVector2D(), Seed);

	if(Possible < SpawnRate)return;

	DevelopeDomains(InChunk);
	PlaceBuildings(InChunk);
	PlacePaths(InChunk);
}

void UVoxelBuildingGenerator::DevelopeDomains(AVoxelChunk* InChunk)
{
	_StartPoint = FVector2D(InChunk->GetWorldIndex().X + 7, InChunk->GetWorldIndex().Y + 7);

	std::priority_queue<
		std::pair<float, FVector2D>,
		std::vector<std::pair<float, FVector2D>>,
		std::greater<std::pair<float, FVector2D>>
	> Points;
	Points.emplace(0, _StartPoint);

	int32 Count = 0;
	const int32 Dx[9] = {1, -1, 0, 0, 1, -1, 1, -1, 0};
	const int32 Dy[9] = {0, 0, 1, -1, 1, -1, -1, 1, 0};

	while(!Points.empty())
	{
		Count++;
		float Cost = Points.top().first;
		FVector2D P = Points.top().second;
		Points.pop();

		for(int d = 0; d < 9; ++d)
		{
			_Domains.Emplace(UMathStatics::Index(P.X + Dx[d], P.Y + Dy[d]));
		}

		if(Cost > 7) break;

		const int32 CenterHeight = Module->GetTopographyByIndex(FIndex(P.X, P.Y)).Height;

		const int32 Dx1[4] = {1, -1, 0, 0};
		const int32 Dy1[4] = {0, 0, 1, -1};

		for(int d = 0; d < 4; ++d)
		{
			int32 x = P.X + Dx1[d] * 3;
			int32 y = P.Y + Dy1[d] * 3;

			if(_Domains.Find(UMathStatics::Index(x, y))) continue;

			int32 Height = Module->GetTopographyByIndex(FIndex(x, y)).Height;
			if(Height <= Module->GetWorldData().SeaLevel || Height > 1000) continue;

			int32 DeltaHeight = FMath::Abs(CenterHeight - Height);

			Points.emplace(Cost + 0.5f + DeltaHeight * 0.5f, FVector2D(x, y));
		}
	}
}

void UVoxelBuildingGenerator::PlaceBuildings(AVoxelChunk* InChunk)
{
	const int32 BuildingSize[3][2] = {{10, 6}, {8, 6}, {6, 6}};
	const int32 Dx[4] = {1, -1, 0, 0};
	const int32 Dy[4] = {0, 0, 1, -1};

	int32 Count = 0;

	std::queue<FVector2D> Points;
	Points.push(_StartPoint);

	while(!Points.empty())
	{
		++Count;

		const auto Pos = Points.front();
		Points.pop();

		const int32 Index = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D() + FVector2D(Count, -Count) * 107, Seed) % 3;
		const int32 Rotate = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D() + FVector2D(Count, -Count) * 17, Seed) % 4;

		if(!PlaceOneBuilding(InChunk, Pos.X, Pos.Y, Index, Rotate) || PlaceOneBuilding(InChunk, Pos.X, Pos.Y, Index,(Rotate + 1) % 4))
		{
			continue;
		}

		int32 Offset = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D() + FVector2D(Count, -Count) * 67, Seed) % 3 + 5;
		int32 OffsetX = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D() + FVector2D(Count, Count) * 61, Seed) % 5 - 2;
		int32 OffsetY = UMathStatics::RandInt(InChunk->GetIndex().ToVector2D() + FVector2D(-Count, Count) * 117, Seed) % 5 - 2;

		for(int i = 0; i < 4; ++i)
		{
			Points.push(FVector2D(Pos.X + Dx[i] *(Offset + BuildingSize[Index][0]) + OffsetX, Pos.Y + Dy[i] *(Offset + BuildingSize[Index][1]) + OffsetY));
		}
	}
}

bool UVoxelBuildingGenerator::PlaceOneBuilding(AVoxelChunk* InChunk, int32 InX, int32 InY, int32 index, int32 InRotate)
{
	const int32 BuildingSize[3][3] = {{10, 6, 7}, {8, 6, 9}, {6, 6, 15}};

	const int RotateIndex = InRotate % 2;
	const int FrontBack = BuildingSize[index][RotateIndex] / 2;
	const int LeftRight = BuildingSize[index][!RotateIndex] / 2;
	const int UpDown = BuildingSize[index][2];

	float Aver = 0;
	for(int i = -FrontBack; i < FrontBack; ++i)
	{
		for(int j = -LeftRight; j < LeftRight; ++j)
		{
			if(!_Domains.Find(UMathStatics::Index(InX + i, InY + j))) return false;

			FIndex Index = FIndex(InX + i, InY + j, Module->GetTopographyByIndex(FIndex(InX + i, InY + j)).Height);
			if(!Module->HasVoxelByIndex(Index, true)) return false;

			Aver += Module->GetTopographyByIndex(FIndex(InX + i, InY + j)).Height;
		}
	}

	Aver /=(BuildingSize[index][0] * BuildingSize[index][1]);
	Aver = floor(Aver + 0.5f);

	if(Aver <= Module->GetWorldData().SeaLevel)return false;

	for(int i = -FrontBack; i < FrontBack; ++i)
	{
		for(int j = -LeftRight; j < LeftRight; ++j)
		{
			for(int k = Module->GetTopographyByIndex(FIndex(InX + i, InY + j)).Height; k <= Aver; ++k)
			{
				FIndex pos = FIndex(InX + i, InY + j, k);
				Module->SetVoxelByIndex(pos, EVoxelType::Cobble_Stone);
			}
			_Domains.Remove(UMathStatics::Index(InX + i, InY + j));
		}
	}

	for(int i = -FrontBack - 1; i < FrontBack + 1; ++i)
	{
		for(int j = -LeftRight - 1; j < LeftRight + 1; ++j)
		{
			for(int k = 0; k < UpDown; ++k)
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

	_Domains.Emplace(UMathStatics::Index(InX - FrontBack, InY - LeftRight));
	_BuildingPos.Push(FVector2D(InX - FrontBack, InY - LeftRight));
	return true;
}

void UVoxelBuildingGenerator::PlacePaths(AVoxelChunk* InChunk)
{
	for(int i = 0; i < _BuildingPos.Num(); ++i)
	{
		for(int j = i + 1; j < _BuildingPos.Num(); ++j)
		{
			const auto Path = PathFinder.FindPath(_BuildingPos[i], _BuildingPos[j]);

			for(FVector2D Pos : Path)
			{
				_Roads.Emplace(UMathStatics::Index(Pos.X, Pos.Y));
				FIndex Index = FIndex(Pos.X, Pos.Y, Module->GetTopographyByIndex(FIndex(Pos.X, Pos.Y)).Height);

				if(!Module->HasVoxelByIndex(Index, true)) continue;

				Module->SetVoxelByIndex(FIndex(Pos.X, Pos.Y, Module->GetTopographyByIndex(FIndex(Pos.X, Pos.Y)).Height), EVoxelType::Cobble_Stone);
			}
		}
	}
	_BuildingPos.Reset();
}

bool UVoxelBuildingGenerator::InBarrier(FVector2D InPos)
{
	return !_Domains.Contains(UMathStatics::Index(InPos.X, InPos.Y));
}

TPair<float, float> UVoxelBuildingGenerator::WeightFormula(FVector2D InStartPos, FVector2D InEndPos, float InCost)
{
	if(_Roads.Contains(UMathStatics::Index(InStartPos.X, InStartPos.Y)))
	{
		InCost -= 0.5f;
	}

	const FVector2D Dist =(InEndPos - InStartPos).GetAbs();
	float Predict =(Dist.X + Dist.Y) * 1.41f - FMath::Max(Dist.X, Dist.Y) * 0.41f + InCost;

	return TPair<float, float>(InCost, Predict);
}
