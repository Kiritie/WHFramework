// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelPlantGenerator.h"

#include "Math/MathStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelPlantGenerator::UVoxelPlantGenerator()
{
}

void UVoxelPlantGenerator::Generate(AVoxelChunk* InChunk)
{
	const int32 cystalSize = 16;

	for (int i = 0; i < Module->GetWorldData().ChunkSize.X; ++i)
		for (int j = 0; j < Module->GetWorldData().ChunkSize.Y; ++j)
		{
			//不允许在空地、石头、沙漠上产生植被
			if (InChunk->GetTopography(FIndex(i, j)).BiomeType == EVoxelBiomeType::None ||
				InChunk->GetTopography(FIndex(i, j)).BiomeType == EVoxelBiomeType::Stone ||
				InChunk->GetTopography(FIndex(i, j)).BiomeType == EVoxelBiomeType::Desert)
			{
				continue;
			}

			// 被挖空（方块ID为0）或者方块ID为9（方块为水）就无法生成
			if (Module->HasVoxelByIndex(FVector(
				InChunk->GetIndex().X * 16 + i,
				InChunk->GetIndex().Y * 16 + j,
				InChunk->GetTopography(FIndex(i, j)).Height)))
			{
				continue;
			}

			//查询地板上一格的方块ID
			//如果存在方块，则无法生成
			if (Module->HasVoxelByIndex(FVector(
				InChunk->GetIndex().X * 16 + i,
				InChunk->GetIndex().Y * 16 + j,
				InChunk->GetTopography(FIndex(i, j)).Height + 1)))
			{
				continue;
			}

			//生成树
			if (GenerateTree(InChunk, i, j, cystalSize))continue;

			//生成草
			if (GenerateFlower(InChunk, i, j, cystalSize))continue;
		}
}

bool UVoxelPlantGenerator::GenerateFlower(AVoxelChunk* InChunk, int32 InX, int32 InY, int32 InCystalSize)
{
	//如果地板方块在水平面以下，则不允许生成花朵
	if (InChunk->GetTopography(FIndex(InX, InY)).Height <= Module->GetWorldData().SeaLevel)
		return false;

	//获得柱块的温度值、湿度值
	float temperature = InChunk->GetTopography(FIndex(InX, InY)).Temperature;
	float humidity = InChunk->GetTopography(FIndex(InX, InY)).Humidity;

	FVector2D pf = FVector2D(float(InX) / Module->GetWorldData().ChunkSize.X / InCystalSize, float(InY) / Module->GetWorldData().ChunkSize.Y / InCystalSize);
	//计算概率值
	float possible = UMathStatics::Rand(pf, Module->GetWorldData().WorldSeed) - FMath::Abs(temperature + 0.1f) * 0.4f + humidity * 0.4f;

	EVoxelType targetID = EVoxelType::Empty;
	//若满足生成花的概率
	if (possible > 1.0f)
	{
		//方块ID：20=黄花、21=蓝花、22=蘑菇、23=蘑菇
		targetID = (EVoxelType)((int32)EVoxelType::Flower_Allium + UMathStatics::RandInt(FVector2D(InX + InY * 21, InY - InX ^ 2), Module->GetWorldData().WorldSeed) % ((int32)EVoxelType::Flower_Allium - (int32)EVoxelType::Flower_Tulip_White));
	}
	//若满足生成草的概率
	else if (possible > 0.85f)
	{
		//方块ID：11=绿草 12=黄草，13=白草
		if (temperature > 0.3f) { targetID = EVoxelType::Tall_Grass; }
		else if (temperature > -0.3f) { targetID = EVoxelType::Tall_Grass; }
		else { targetID = EVoxelType::Tall_Grass; }
	}
	if (targetID == EVoxelType::Empty)
		return false;

	//添加目标方块（花或草）    
	Module->SetVoxelByIndex(FVector(
		                        InChunk->GetIndex().X * 16 + InX,
		                        InChunk->GetIndex().Y * 16 + InY,
		                        InChunk->GetTopography(FIndex(InX, InY)).Height + 1), targetID);

	return true;
}

bool UVoxelPlantGenerator::GenerateTree(AVoxelChunk* InChunk, int32 InX, int32 InY, int32 InCystalSize)
{
	//如果地板方块在水平面下一格的以下，则不允许生成花朵
	if (InChunk->GetTopography(FIndex(InX, InY)).Height <= Module->GetWorldData().SeaLevel - 1)return false;

	FVector2D pf = FVector2D(float(InX) / Module->GetWorldData().ChunkSize.X / InCystalSize, float(InY) / Module->GetWorldData().ChunkSize.Y / InCystalSize);
	float temperature = InChunk->GetTopography(FIndex(InX, InY)).Temperature;
	float humidity = InChunk->GetTopography(FIndex(InX, InY)).Humidity;
	float possible = (Module->GetNoise2D(pf, FVector(1.f, 1.f, 1.f)) + 1.0f) / 2.0f * 0.15f - FMath::Abs(temperature + 0.1f) * 0.10f + humidity * 0.15f + UMathStatics::Rand(-pf, Module->GetWorldData().WorldSeed) * 0.9f;

	//若满足概率
	if (possible < 0.985f)return false;
	//树干高度
	int32 treeHeight = (UMathStatics::RandInt(FVector2D::UnitVector - pf, Module->GetWorldData().WorldSeed) % 3) + 4;

	//根据温度选择树类型
	temperature += (UMathStatics::Rand(FVector2D::UnitVector + pf, Module->GetWorldData().WorldSeed) - 0.5f) * 0.2f;

	EVoxelType targetWoodID;
	EVoxelType targetLeafID;
	//方块ID：14=温带木 15=热带木，16=寒带木，17=温带树叶 18=寒带树叶，19=热带树叶
	if (temperature > 0.3f)
	{
		targetWoodID = EVoxelType::Oak;
		targetLeafID = EVoxelType::Oak_Leaves;
	}
	else if (temperature > -0.3f)
	{
		targetWoodID = EVoxelType::Oak;
		targetLeafID = EVoxelType::Oak_Leaves;
	}
	else
	{
		targetWoodID = EVoxelType::Birch;
		targetLeafID = EVoxelType::Birch_Leaves;
	}

	for (int k = 0; k < treeHeight; ++k)
	{
		//生成目标方块（树干）    
		Module->SetVoxelByIndex(FVector(
			                        InChunk->GetIndex().X * 16 + InX,
			                        InChunk->GetIndex().Y * 16 + InY,
			                        InChunk->GetTopography(FIndex(InX, InY)).Height + 1 + k), targetWoodID);
	}

	int32 t1 = UMathStatics::Rand(17 * pf, Module->GetWorldData().WorldSeed) * 4 + 1.5f + int32(treeHeight >= 5);
	int32 t2 = UMathStatics::Rand(11 * pf, Module->GetWorldData().WorldSeed) * 4 + 1.5f + int32(treeHeight >= 5);
	int32 leafHeight = treeHeight + 1 + t1 % 3;
	int32 initLeafHeight = 2 + t2 % 2;
	for (int k = leafHeight - 1; k >= initLeafHeight; --k)
	{
		//贝塞尔曲线计算树叶半径
		float leafRadius =
			UMathStatics::Bezier(
				FVector2D(0, 0)
				, FVector2D(0.33f, t1)
				, FVector2D(0.66f, t2)
				, FVector2D(1, 0)
				, float(k - initLeafHeight) / (leafHeight - 1 - initLeafHeight)).Y;

		//生成树叶
		GenerateLeaves(InChunk, InX, InY, InChunk->GetTopography(FIndex(InX, InY)).Height + 1 + k, leafRadius, targetLeafID);
	}

	return true;
}

void UVoxelPlantGenerator::GenerateLeaves(AVoxelChunk* InChunk, int32 x, int32 y, int32 InHeight, int32 InRadius, EVoxelType InTargetLeafID)
{
	InRadius = FMath::Clamp(InRadius, 0, 3);

	for (int i = 0; i < 5; ++i)
		for (int j = 0; j < 5; ++j)
		{
			if (!LeavesTemplate[InRadius][InChunk->LocalIndexToWorld(FIndex(i, j)).ToInt64()])
				continue;

			int32 dx = x + i - 2;
			int32 dy = y + j - 2;
			int32 gx = dx + InChunk->GetIndex().X * 16;
			int32 gy = dy + InChunk->GetIndex().Y * 16;

			if (Module->HasVoxelByIndex(FVector(gx, gy, InHeight)))
				continue;

			Module->SetVoxelByIndex(FVector(gx, gy, InHeight), InTargetLeafID);

			//树顶雪
			// if(Module->BlocksBiome[InChunk->LocalIndexToWorld(FIndex(x, y)).ToInt64()]==1 && UMathStatics::rand(FVector2D(gx+height*11,gy*17+radius*23))>0.13f){
			//     Module->SetVoxelByIndex(FVector(gx,gy,height+1),24);
			// }
		}
}

bool UVoxelPlantGenerator::LeavesTemplate[4][5][5] = {
	{
		{0, 0, 0, 0, 0},
		{0, 0, 1, 0, 0},
		{0, 1, 1, 1, 0},
		{0, 0, 1, 0, 0},
		{0, 0, 0, 0, 0}
	},

	{
		{0, 0, 0, 0, 0},
		{0, 1, 1, 1, 0},
		{0, 1, 1, 1, 0},
		{0, 1, 1, 1, 0},
		{0, 0, 0, 0, 0}
	},

	{
		{0, 1, 1, 1, 0},
		{1, 1, 1, 1, 1},
		{1, 1, 1, 1, 1},
		{1, 1, 1, 1, 1},
		{0, 1, 1, 1, 0}
	},

	{
		{1, 1, 1, 1, 1},
		{1, 1, 1, 1, 1},
		{1, 1, 1, 1, 1},
		{1, 1, 1, 1, 1},
		{1, 1, 1, 1, 1}
	}
};
