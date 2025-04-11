// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "Math/MathTypes.h"
#include "Tool/PathFinder.h"
#include "VoxelVillageGenerator.generated.h"

class UVoxelPrefabData;
/**
 *
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelVillageGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	UVoxelVillageGenerator();

public:
	virtual void Initialize(UVoxelModule* InModule) override;
	
	virtual void Generate(AVoxelChunk* InChunk) override;

public:
	void DevelopeDomains(AVoxelChunk* InChunk);

	void PlaceBuildings(AVoxelChunk* InChunk);

	void PlacePaths();

	//检查是否有障碍物（不可走）
	bool InBarrier(FVector2D InPos);

	//计算权值公式
	TPair<float, float> WeightFormula(FVector2D InStartPos, FVector2D InEndPos, float InCost);

	bool PlaceOneBuilding(int32 InX, int32 InY, int32 InIndex, int32 InRotate);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpawnRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FPrimaryAssetId> PrefabAssets;

	FPathFinder PathFinder;
	
private:
	//发展域初始中心点
	FVector2D _StartPoint;
	//发展域
	TSet<uint64> _Domains;
	//已存在道路
	TSet<uint64> _Roads;
	//所有建筑门口点
	TArray<FVector2D> _BuildingPos;
	//所有建筑预制体
	TArray<UVoxelPrefabData*> _PrefabAssets;
};
