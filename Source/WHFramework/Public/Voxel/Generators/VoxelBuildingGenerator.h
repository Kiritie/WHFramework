// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
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
	bool InBarrier(FVector2D InLocation);

	//计算权值公式
	TPair<float, float> WeightFormula(FVector2D InStartLocation, FVector2D InEndLocation, float InCost);

	bool PlaceOneBuilding(AVoxelChunk* InChunk, int32 InX, int32 InY, int32 InBuildingIndex, int32 InRotate);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpawnRate;
	
	//发展域
	TSet<uint64> Domains;
	//已存在道路
	TSet<uint64> Roads;
	//发展域初始中心点
	FVector2D StartPoint;
	//所有建筑门口点
	TArray<FVector2D> BuildingPos;
};
