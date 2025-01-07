// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "Tool/PathFinder.h"
#include "VoxelBuildingGenerator.generated.h"

/**
 *
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelBuildingGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	UVoxelBuildingGenerator();

public:
	virtual void Generate(AVoxelChunk* InChunk) override;

public:
	void DevelopeDomains(AVoxelChunk* InChunk);

	void PlaceBuildings(AVoxelChunk* InChunk);

	void PlacePaths(AVoxelChunk* InChunk);

	//检查是否有障碍物（不可走）
	bool InBarrier(FVector2D InPos);

	//计算权值公式
	TPair<float, float> WeightFormula(FVector2D InStartPos, FVector2D InEndPos, float InCost);

	bool PlaceOneBuilding(AVoxelChunk* InChunk, int32 InX, int32 InY, int32 InBuildingIndex, int32 InRotate);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpawnRate;

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
};
