// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/PlantGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/Chunks/VoxelChunk.h"

void PlantGenerator::GeneratePlant(AVoxelChunk* chunk, UVoxelModule* info)
{
	const int32 cystalSize = 16;
	int32 seedOffset = UVoxelModuleStatics::hash21(chunk->GetIndex().ToVector2D());

	UVoxelModuleStatics::prehandleSimplexNoise(chunk->GetIndex().ToVector2D(), cystalSize);
	UVoxelModuleStatics::setSeed(1317 + seedOffset);

	for (int i = 0; i < UVoxelModule::Get().GetWorldData().ChunkSize.X; ++i)
		for (int j = 0; j < UVoxelModule::Get().GetWorldData().ChunkSize.Y; ++j)
		{
			//不允许在空地、石头、沙漠上产生植被
			if (chunk->GetTopography(FIndex(i, j)).BiomeType == EVoxelBiomeType::None ||
				chunk->GetTopography(FIndex(i, j)).BiomeType == EVoxelBiomeType::Stone ||
				chunk->GetTopography(FIndex(i, j)).BiomeType == EVoxelBiomeType::Desert)
			{
				continue;
			}

			// 被挖空（方块ID为0）或者方块ID为9（方块为水）就无法生成
			if(info->HasVoxelByIndex(FVector(
					chunk->GetIndex().X*16+i,
					chunk->GetIndex().Y*16+j,
					chunk->GetTopography(FIndex(i, j)).Height))){
			     continue;
			 }
			
			 //查询地板上一格的方块ID
			 //如果存在方块，则无法生成
			if(info->HasVoxelByIndex(FVector(
				   chunk->GetIndex().X*16+i,
				   chunk->GetIndex().Y*16+j,
				   chunk->GetTopography(FIndex(i, j)).Height+1))){
			     continue;
			 }

			//生成树
			if (GenerateTree(chunk, info, i, j, cystalSize))continue;

			//生成草
			if (GenerateFlower(chunk, info, i, j, cystalSize))continue;
		}
}

bool PlantGenerator::GenerateFlower(AVoxelChunk* chunk, UVoxelModule* info, int32 i, int32 j, int32 cystalSize)
{
	//如果地板方块在水平面以下，则不允许生成花朵
	if (chunk->GetTopography(FIndex(i, j)).Height <= SeaLevel)
		return false;

	//获得柱块的温度值、湿度值
	float temperature = chunk->GetTopography(FIndex(i, j)).Temperature;
	float humidity = chunk->GetTopography(FIndex(i, j)).Humidity;

	FVector2D pf = FVector2D(float(i) / UVoxelModule::Get().GetWorldData().ChunkSize.X / cystalSize, float(j) / UVoxelModule::Get().GetWorldData().ChunkSize.Y / cystalSize);
	//计算概率值
	float possible = UVoxelModuleStatics::rand(pf) - FMath::Abs(temperature + 0.1f) * 0.4f + humidity * 0.4f;

	EVoxelType targetID = EVoxelType::Empty;
	//若满足生成花的概率
	if (possible > 1.0f)
	{
		//方块ID：20=黄花、21=蓝花、22=蘑菇、23=蘑菇
		// targetID = 20 + UVoxelModuleStatics::randInt(FVector2D(i + j * 21, j - i ^ 2)) % 4;
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
	info->SetVoxelByIndex(FVector(
	    chunk->GetIndex().X*16+i,
	    chunk->GetIndex().Y*16+j,
	    chunk->GetTopography(FIndex(i, j)).Height+1),targetID);

	return true;
}

bool PlantGenerator::GenerateTree(AVoxelChunk* chunk, UVoxelModule* info, int32 i, int32 j, int32 cystalSize)
{
	//如果地板方块在水平面下一格的以下，则不允许生成花朵
	if (chunk->GetTopography(FIndex(i, j)).Height <= SeaLevel - 1)return false;

	FVector2D pf = FVector2D(float(i) / UVoxelModule::Get().GetWorldData().ChunkSize.X / cystalSize, float(j) / UVoxelModule::Get().GetWorldData().ChunkSize.Y / cystalSize);
	float temperature = chunk->GetTopography(FIndex(i, j)).Temperature;
	float humidity = chunk->GetTopography(FIndex(i, j)).Humidity;
	float possible = (UVoxelModuleStatics::simplexNoise(pf) + 1.0f) / 2.0f * 0.15f - FMath::Abs(temperature + 0.1f) * 0.10f + humidity * 0.15f + UVoxelModuleStatics::rand(-pf) * 0.9f;

	//若满足概率
	if (possible < 0.985f)return false;
	//树干高度
	int32 treeHeight = (UVoxelModuleStatics::randInt(FVector2D::UnitVector - pf) % 3) + 4;

	//根据温度选择树类型
	temperature += (UVoxelModuleStatics::rand(FVector2D::UnitVector + pf) - 0.5f) * 0.2f;

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
		 info->SetVoxelByIndex(FVector(
		     chunk->GetIndex().X*16+i,
		     chunk->GetIndex().Y*16+j,
		     chunk->GetTopography(FIndex(i, j)).Height+1+k),targetWoodID);
	}

	int32 t1 = UVoxelModuleStatics::rand(17 * pf) * 4 + 1.5f + int32(treeHeight >= 5);
	int32 t2 = UVoxelModuleStatics::rand(11 * pf) * 4 + 1.5f + int32(treeHeight >= 5);
	int32 leafHeight = treeHeight + 1 + t1 % 3;
	int32 initLeafHeight = 2 + t2 % 2;
	for (int k = leafHeight - 1; k >= initLeafHeight; --k)
	{
		//贝塞尔曲线计算树叶半径
		float leafRadius =
			UVoxelModuleStatics::bezier(
				FVector2D(0, 0)
				, FVector2D(0.33f, t1)
				, FVector2D(0.66f, t2)
				, FVector2D(1, 0)
				, float(k - initLeafHeight) / (leafHeight - 1 - initLeafHeight)).Y;

		//生成树叶
		GenerateLeaves(chunk, info, i, j, chunk->GetTopography(FIndex(i, j)).Height + 1 + k, leafRadius, targetLeafID);
	}

	return true;
}

void PlantGenerator::GenerateLeaves(AVoxelChunk* chunk, UVoxelModule* info, int32 x, int32 y, int32 height, int32 radius, EVoxelType targetLeafID)
{
	radius = FMath::Clamp(radius, 0, 3);

	for (int i = 0; i < 5; ++i)
		for (int j = 0; j < 5; ++j)
		{
			if (!leavesTemplate[radius][chunk->LocalIndexToWorld(FIndex(i, j)).ToInt64()])
				continue;

			int32 dx = x + i - 2;
			int32 dy = y + j - 2;
			int32 gx = dx + chunk->GetIndex().X * 16;
			int32 gy = dy + chunk->GetIndex().Y * 16;

			if(info->HasVoxelByIndex(FVector(gx,gy,height)))
			    continue;
			
			info->SetVoxelByIndex(FVector(gx,gy,height),targetLeafID);
			
			//树顶雪
			// if(info->BlocksBiome[chunk->LocalIndexToWorld(FIndex(x, y)).ToInt64()]==1 && UVoxelModuleStatics::rand(FVector2D(gx+height*11,gy*17+radius*23))>0.13f){
			//     info->SetVoxelByIndex(FVector(gx,gy,height+1),24);
			// }
		}
}

bool PlantGenerator::leavesTemplate[4][5][5] = {
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
