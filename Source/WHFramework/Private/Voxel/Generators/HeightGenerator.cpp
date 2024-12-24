// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Generators/HeightGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

void HeightGenerator::GenerateHeight(AVoxelChunk* chunk,UVoxelModule* info)
{
	for (int i = 0; i < UVoxelModule::Get().GetWorldData().ChunkSize.X; ++i)
	for (int j = 0; j < UVoxelModule::Get().GetWorldData().ChunkSize.Y; ++j)
	{
		const int32 BiomeHeight = UVoxelModule::Get().GetTerrainHeight(chunk->LocalIndexToWorld(FIndex(i, j)));
		chunk->GetTopography(FIndex(i, j)).Height = BiomeHeight;
	}
	//
	// //晶格大小
	// int32 cystalSize[3] = {4,16,64};
	// //权重
	// float weight[3] = {0.12f,0.22f,0.66f};
	// //波幅
	// float maxHeigh[3] = {64.0f,64.0f,64.0f};
	//
	// for (int d = 0; d < 3; ++d) {
	//
	// 	UVoxelModuleStatics::prehandleSimplexNoise(chunk->ChunkPosition,cystalSize[d]);
	//
	// 	for (int i = 0; i < UVoxelModule::Get().GetWorldData().ChunkSize.X; ++i)
	// 	for (int j = 0; j < UVoxelModule::Get().GetWorldData().ChunkSize.Y; ++j)
	// 	{
	// 		FVector2D pf = FVector2D(float(i) / UVoxelModule::Get().GetWorldData().ChunkSize.X / cystalSize[d], float(j) / UVoxelModule::Get().GetWorldData().ChunkSize.Y / cystalSize[d]);
	// 		
	// 		//chunk->GetTopography(FIndex(i, j)).Height += (UVoxelModuleStatics::perlinNoise(pf)*maxHeigh[d] + maxHeigh[d])*weight[d];
	// 		chunk->GetTopography(FIndex(i, j)).Height += (UVoxelModuleStatics::simplexNoise(pf)*maxHeigh[d] + maxHeigh[d])*weight[d];
	// 	}
	// }
}
	
