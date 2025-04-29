// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelVillageGenerator.h"

#include "Asset/AssetModuleStatics.h"
#include "Math/MathHelper.h"
#include "Scene/SceneModuleStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "Voxel/Voxels/Data/VoxelData.h"

UVoxelVillageGenerator::UVoxelVillageGenerator()
{
	Seed = 453;
	SpawnRate = 0.999f;

	PathFinder = FPathFinder();
	PathFinder.SetConditionInBarrier([this](FVector2D Pos) { return InBarrier(Pos); });
	PathFinder.SetWeightFormula([this](FVector2D StartPos, FVector2D EndPos, float Cost) { return WeightFormula(StartPos, EndPos, Cost); });

	PrefabAssets = TArray<FPrimaryAssetId>();
	PrefabAssets.Add(FPrimaryAssetId(TEXT("VoxelPrefab:DA_Building_1")));
	PrefabAssets.Add(FPrimaryAssetId(TEXT("VoxelPrefab:DA_Building_2")));
	PrefabAssets.Add(FPrimaryAssetId(TEXT("VoxelPrefab:DA_Building_3")));
	
	_StartPoint = FVector2D::ZeroVector;
	_Domains = {};
	_Roads = {};
	_BuildingPos = {};
}

void UVoxelVillageGenerator::Initialize(UVoxelModule* InModule)
{
	Super::Initialize(InModule);

	for(auto& Iter : PrefabAssets)
	{
		_PrefabAssets.Add(UAssetModuleStatics::LoadPrimaryAsset<UVoxelPrefabData>(Iter));
	}
}

void UVoxelVillageGenerator::Generate(AVoxelChunk* InChunk)
{
	float Possible = FMathHelper::HashRand(InChunk->GetIndex().ToVector2D(), Seed);
	if(Possible < SpawnRate) return;

	_Domains.Reset();
	_Roads.Reset();
	_BuildingPos.Empty();

	DevelopeDomains(InChunk);
	PlaceBuildings(InChunk);
	PlacePaths();

	FWorldMaxMapArea MaxMapArea;
	MaxMapArea.AreaDisplayName = FText::FromString(TEXT("村庄"));
	MaxMapArea.AreaType = (EWorldMaxMapAreaType)EVoxelWorldMaxMapAreaType::Village;
	MaxMapArea.AreaShape = EWorldMaxMapAreaShape::Ellipse;
	MaxMapArea.AreaCenter = _StartPoint;
	MaxMapArea.AreaRadius = FVector2D(30.f);
	
	USceneModuleStatics::AddMaxMapArea(MaxMapArea);
}

void UVoxelVillageGenerator::DevelopeDomains(AVoxelChunk* InChunk)
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
			_Domains.Emplace(FMathHelper::Index(P.X + Dx[d], P.Y + Dy[d]));
		}

		if(Cost > 7) break;

		const int32 CenterHeight = Module->GetTopographyByIndex(FIndex(P.X, P.Y)).Height;

		const int32 Dx1[4] = {1, -1, 0, 0};
		const int32 Dy1[4] = {0, 0, 1, -1};

		for(int d = 0; d < 4; ++d)
		{
			int32 x = P.X + Dx1[d] * 3;
			int32 y = P.Y + Dy1[d] * 3;

			if(_Domains.Find(FMathHelper::Index(x, y))) continue;

			int32 Height = Module->GetTopographyByIndex(FIndex(x, y)).Height;
			if(Height <= Module->GetWorldData().SeaLevel || Height > 1000) continue;

			int32 DeltaHeight = FMath::Abs(CenterHeight - Height);

			Points.emplace(Cost + 0.5f + DeltaHeight * 0.5f, FVector2D(x, y));
		}
	}
}

void UVoxelVillageGenerator::PlaceBuildings(AVoxelChunk* InChunk)
{
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

		const int32 Index = FMathHelper::HashRandInt(InChunk->GetIndex().ToVector2D() + FVector2D(Count, -Count) * 107, Seed) % _PrefabAssets.Num();
		const int32 Rotate = FMathHelper::HashRandInt(InChunk->GetIndex().ToVector2D() + FVector2D(Count, -Count) * 17, Seed) % 4;

		if(!PlaceOneBuilding(Pos.X, Pos.Y, Index, Rotate) || PlaceOneBuilding(Pos.X, Pos.Y, Index, ((Rotate + 1) % 4)))
		{
			continue;
		}

		const int32 Offset = FMathHelper::HashRandInt(InChunk->GetIndex().ToVector2D() + FVector2D(Count, -Count) * 67, Seed) % 3 + 5;
		const int32 OffsetX = FMathHelper::HashRandInt(InChunk->GetIndex().ToVector2D() + FVector2D(Count, Count) * 61, Seed) % 5 - 2;
		const int32 OffsetY = FMathHelper::HashRandInt(InChunk->GetIndex().ToVector2D() + FVector2D(-Count, Count) * 117, Seed) % 5 - 2;

		for(int i = 0; i < 4; ++i)
		{
			Points.push(FVector2D(Pos.X + Dx[i] * (Offset + _PrefabAssets[Index]->VoxelSize.X) + OffsetX, Pos.Y + Dy[i] * (Offset + _PrefabAssets[Index]->VoxelSize.Y) + OffsetY));
		}
	}
}

bool UVoxelVillageGenerator::PlaceOneBuilding(int32 InX, int32 InY, int32 InIndex, int32 InRotate)
{
	const int RotateIndex = InRotate % 2;
	const int FrontBack = _PrefabAssets[InIndex]->VoxelSize[RotateIndex] / 2;
	const int LeftRight = _PrefabAssets[InIndex]->VoxelSize[!RotateIndex] / 2;
	const int UpDown = _PrefabAssets[InIndex]->VoxelSize[2];

	float Aver = 0;
	for(int i = -FrontBack; i < FrontBack; ++i)
	{
		for(int j = -LeftRight; j < LeftRight; ++j)
		{
			if(!_Domains.Find(FMathHelper::Index(InX + i, InY + j))) return false;

			FIndex Index = FIndex(InX + i, InY + j, Module->GetTopographyByIndex(FIndex(InX + i, InY + j)).Height);
			if(!Module->HasVoxelByIndex(Index, true)) return false;

			Aver += Module->GetTopographyByIndex(FIndex(InX + i, InY + j)).Height;
		}
	}

	Aver /= _PrefabAssets[InIndex]->VoxelSize[0] * _PrefabAssets[InIndex]->VoxelSize[1];
	Aver = floor(Aver + 0.5f);

	if(Aver <= Module->GetWorldData().SeaLevel) return false;

	for(int i = -FrontBack; i < FrontBack; ++i)
	{
		for(int j = -LeftRight; j < LeftRight; ++j)
		{
			for(int k = Module->GetTopographyByIndex(FIndex(InX + i, InY + j)).Height; k <= Aver; ++k)
			{
				const FIndex Index = FIndex(InX + i, InY + j, k);
				Module->SetVoxelByIndex(Index, EVoxelType::Cobble_Stone);
			}
			_Domains.Remove(FMathHelper::Index(InX + i, InY + j));
		}
	}

	for(int i = -FrontBack - 1; i < FrontBack + 1; ++i)
	{
		for(int j = -LeftRight - 1; j < LeftRight + 1; ++j)
		{
			for(int k = 0; k < UpDown; ++k)
			{
				const FIndex Index = FIndex(InX + i, InY + j, Aver + k + 1);
				Module->SetVoxelByIndex(Index, FVoxelItem::Empty, true);
			}
		}
	}

	TArray<FString> VoxelDatas;
	_PrefabAssets[InIndex]->VoxelDatas.ParseIntoArray(VoxelDatas, TEXT("|"));
	for(auto& Iter : VoxelDatas)
	{
		FVoxelItem VoxelItem = FVoxelItem(Iter, true);
		if(VoxelItem.GetVoxelData().bRotatable)
		{
			VoxelItem.Angle = FMathHelper::CombineRightAngle(VoxelItem.Angle, (ERightAngle)InRotate);
		}
		const FIndex Index = FIndex(InX, InY, Aver) + FMathHelper::RotateIndex(VoxelItem.Index, (ERightAngle)InRotate) + UVoxelModuleStatics::RightAngleToVoxelIndex((ERightAngle)InRotate);
		Module->SetVoxelByIndex(Index, VoxelItem);
	}

	_Domains.Emplace(FMathHelper::Index(InX - FrontBack, InY - LeftRight));
	_BuildingPos.Push(FVector2D(InX - FrontBack, InY - LeftRight));

	return true;
}

void UVoxelVillageGenerator::PlacePaths()
{
	for(int i = 0; i < _BuildingPos.Num(); ++i)
	{
		for(int j = i + 1; j < _BuildingPos.Num(); ++j)
		{
			const auto Path = PathFinder.FindPath(_BuildingPos[i], _BuildingPos[j]);

			for(FVector2D Pos : Path)
			{
				_Roads.Emplace(FMathHelper::Index(Pos.X, Pos.Y));
				const FIndex Index = FIndex(Pos.X, Pos.Y, Module->GetTopographyByIndex(FIndex(Pos.X, Pos.Y)).Height);

				if(!Module->HasVoxelByIndex(Index, true)) continue;

				Module->SetVoxelByIndex(FIndex(Pos.X, Pos.Y, Module->GetTopographyByIndex(FIndex(Pos.X, Pos.Y)).Height), EVoxelType::Cobble_Stone);
			}
		}
	}
	_BuildingPos.Reset();
}

bool UVoxelVillageGenerator::InBarrier(FVector2D InPos)
{
	return !_Domains.Contains(FMathHelper::Index(InPos.X, InPos.Y));
}

TPair<float, float> UVoxelVillageGenerator::WeightFormula(FVector2D InStartPos, FVector2D InEndPos, float InCost)
{
	if(_Roads.Contains(FMathHelper::Index(InStartPos.X, InStartPos.Y)))
	{
		InCost -= 0.5f;
	}

	const FVector2D Dist =(InEndPos - InStartPos).GetAbs();
	float Predict =(Dist.X + Dist.Y) * 1.41f - FMath::Max(Dist.X, Dist.Y) * 0.41f + InCost;

	return TPair<float, float>(InCost, Predict);
}
