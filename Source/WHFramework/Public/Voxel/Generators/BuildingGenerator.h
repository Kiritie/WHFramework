// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UVoxelModule;
class AVoxelChunk;
/**
 * 
 */
class WHFRAMEWORK_API BuildingGenerator
{
public:
	//发展域
	static TSet<uint64> domains;
	//已存在道路
	static TSet<uint64> roads;
	//发展域初始中心点
	static FVector2D startPoint;
	//所有建筑门口点
	static TArray<FVector2D> buildingPos;

	static void GenerateBuilding(AVoxelChunk* chunk,UVoxelModule* info);

	static void DevelopeDomains(AVoxelChunk* chunk,UVoxelModule* info);

	static void PlaceBuildings(AVoxelChunk* chunk,UVoxelModule* info);

	static void PlacePaths(AVoxelChunk* chunk,UVoxelModule* info);

	//检查是否有障碍物（不可走）
	static bool inBarrier(FVector2D pos);
	
	//计算权值公式
	static TPair<float,float> weightFormula(FVector2D pos,FVector2D endPos,float cost);

private:
	static bool PlaceOneBuilding(UVoxelModule* info,int32 x,int32 y,int32 buildingIndex,int32 rotate);

private:
	BuildingGenerator() = delete;
	~BuildingGenerator() = delete;
};
