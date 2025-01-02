// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelFoliageGenerator.h"

#include "Math/MathStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelFoliageGenerator::UVoxelFoliageGenerator()
{
	Seed = 1317;
	CrystalSize = 16.f;
	GrassRate = 0.9f;
	FlowerRate = 0.98f;
	TreeRate = 0.99f;
}

void UVoxelFoliageGenerator::Generate(AVoxelChunk* InChunk)
{
	_Seed = UMathStatics::Hash21(InChunk->GetIndex().ToVector2D()) + Seed;
	
	ITER_INDEX2D(Index, Module->GetWorldData().ChunkSize, false,
		const FVoxelTopography& Topography = InChunk->GetTopography(Index);
		//不允许在空地、石头、沙漠上产生植被
		if (Topography.BiomeType == EVoxelBiomeType::None ||
			Topography.BiomeType == EVoxelBiomeType::Stone ||
			Topography.BiomeType == EVoxelBiomeType::Desert) continue;

		const FIndex GlobalIndex = InChunk->LocalIndexToWorld(Index);
		// 被挖空（方块ID为0）或者方块ID为9（方块为水）就无法生成
		if (!Module->HasVoxelByIndex(FIndex(GlobalIndex.X, GlobalIndex.Y, Topography.Height))) continue;

		//查询地板上一格的方块ID
		//如果存在方块，则无法生成
		if (Module->HasVoxelByIndex(FIndex(GlobalIndex.X, GlobalIndex.Y, Topography.Height + 1))) continue;

		//生成树
		if (GenerateTree(InChunk, Index, CrystalSize)) continue;

		//生成草
		GeneratePlant(InChunk, Index, CrystalSize);
	)
}

bool UVoxelFoliageGenerator::GeneratePlant(AVoxelChunk* InChunk, FIndex InIndex, int32 InCrystalSize)
{
	const FVoxelTopography& Topography = InChunk->GetTopography(InIndex);
	
	//如果地板方块在水平面以下，则不允许生成花朵
	if (Topography.Height <= Module->GetWorldData().SeaLevel) return false;

	//获得柱块的温度值、湿度值
	float Temperature = Topography.Temperature;
	float Humidity = Topography.Humidity;

	FVector2D Location = FVector2D(float(InIndex.X) / Module->GetWorldData().ChunkSize.X / InCrystalSize, float(InIndex.Y) / Module->GetWorldData().ChunkSize.Y / InCrystalSize);
	//计算概率值
	float Possible = UMathStatics::Rand(Location, _Seed) - FMath::Abs(Temperature + 0.1f) * 0.5f + Humidity * 0.4f;

	EVoxelType VoxelType = EVoxelType::Empty;
	//若满足生成花的概率
	if (Possible > FlowerRate)
	{
		VoxelType = (EVoxelType)((int32)EVoxelType::Flower_Allium + UMathStatics::RandInt(FVector2D(InIndex.X + InIndex.Y * 21, InIndex.Y - InIndex.X ^ 2), _Seed) % ((int32)EVoxelType::Flower_Tulip_White - (int32)EVoxelType::Flower_Allium));
	}
	//若满足生成草的概率
	else if (Possible > GrassRate)
	{
		//方块ID：11=绿草 12=黄草，13=白草
		if (Temperature > 0.3f)
		{
			VoxelType = EVoxelType::Tall_Grass;
		}
		else if (Temperature > -0.3f)
		{
			VoxelType = EVoxelType::Tall_Grass;
		}
		else
		{
			VoxelType = EVoxelType::Tall_Grass;
		}
	}
	if (VoxelType != EVoxelType::Empty)
	{
		//添加目标方块（花或草）    
		Module->SetVoxelByIndex(InChunk->LocalIndexToWorld(FIndex(InIndex.X, InIndex.Y, Topography.Height + 1)), VoxelType);
	}
	return true;
}

bool UVoxelFoliageGenerator::GenerateTree(AVoxelChunk* InChunk, FIndex InIndex, int32 InCrystalSize)
{
	const FVoxelTopography& Topography = InChunk->GetTopography(InIndex);
	
	//如果地板方块在水平面下一格的以下，则不允许生成花朵
	if (Topography.Height <= Module->GetWorldData().SeaLevel - 1) return false;

	const FVector2D Location = FVector2D(float(InIndex.X) / Module->GetWorldData().ChunkSize.X / InCrystalSize, float(InIndex.Y) / Module->GetWorldData().ChunkSize.Y / InCrystalSize);
	float Temperature = Topography.Temperature;
	float Humidity = Topography.Humidity;
	float Possible = UMathStatics::Rand(-Location, _Seed) * 0.9f + Module->GetVoxelNoise2D(Location, false, true) * 0.15f - FMath::Abs(Temperature + 0.1f) * 0.15f + Humidity * 0.15f;

	//若满足概率
	if (Possible <= TreeRate) return false;
	//树干高度
	const int32 TreeHeight = (UMathStatics::RandInt(FVector2D::UnitVector - Location, _Seed) % 3) + 4;

	//根据温度选择树类型
	Temperature += (UMathStatics::Rand(FVector2D::UnitVector + Location, _Seed) - 0.5f) * 0.2f;

	EVoxelType WoodType;
	EVoxelType LeafType;
	//方块ID：14=温带木 15=热带木，16=寒带木，17=温带树叶 18=寒带树叶，19=热带树叶
	if (Temperature > 0.3f)
	{
		WoodType = EVoxelType::Oak;
		LeafType = EVoxelType::Oak_Leaves;
	}
	else if (Temperature > -0.3f)
	{
		WoodType = EVoxelType::Oak;
		LeafType = EVoxelType::Oak_Leaves;
	}
	else
	{
		WoodType = EVoxelType::Birch;
		LeafType = EVoxelType::Birch_Leaves;
	}

	for (int i = 0; i < TreeHeight; ++i)
	{
		//生成目标方块（树干）    
		Module->SetVoxelByIndex(InChunk->LocalIndexToWorld(FIndex(InIndex.X, InIndex.Y, Topography.Height + 1 + i)), WoodType);
	}

	const int32 T1 = UMathStatics::Rand(17 * Location, _Seed) * 4 + 1.5f + int32(TreeHeight >= 5);
	const int32 T2 = UMathStatics::Rand(11 * Location, _Seed) * 4 + 1.5f + int32(TreeHeight >= 5);
	const int32 LeafHeight = TreeHeight + 1 + T1 % 3;
	const int32 InitLeafHeight = 2 + T2 % 2;
	for (int i = LeafHeight - 1; i >= InitLeafHeight; --i)
	{
		//贝塞尔曲线计算树叶半径
		const float LeafRadius = UMathStatics::Bezier(FVector2D(0, 0), FVector2D(0.33f, T1), FVector2D(0.66f, T2), FVector2D(1, 0), float(i - InitLeafHeight) / (LeafHeight - 1 - InitLeafHeight)).Y;
		//生成树叶
		GenerateLeaves(InChunk, InIndex, Topography.Height + 1 + i, LeafRadius, LeafType);
	}
	return true;
}

void UVoxelFoliageGenerator::GenerateLeaves(AVoxelChunk* InChunk, FIndex InIndex, int32 InHeight, int32 InRadius, EVoxelType InLeafType)
{
	InRadius = FMath::Clamp(InRadius, 0, 3);

	for (int i = 0; i < 5; ++i)
	{
		for (int j = 0; j < 5; ++j)
		{
			if (!LeavesTemplate[InRadius][InChunk->LocalIndexToWorld(FIndex(i, j)).ToInt64()]) continue;

			const FIndex Index =  InChunk->LocalIndexToWorld(FIndex(InIndex.X + i - 2, InIndex.Y + j - 2, InHeight));
			if (!Module->HasVoxelByIndex(Index))
			{
				Module->SetVoxelByIndex(Index, InLeafType);
			}
		}
	}
}

bool UVoxelFoliageGenerator::LeavesTemplate[4][5][5] = {
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
