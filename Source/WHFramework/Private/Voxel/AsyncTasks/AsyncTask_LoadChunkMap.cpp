// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/AsyncTasks/AsyncTask_LoadChunkMap.h"

#include "Global/GlobalBPLibrary.h"
#include "Voxel/VoxelModule.h"

AsyncTask_LoadChunkMap::AsyncTask_LoadChunkMap(AVoxelModule* InVoxelModule, TArray<FIndex> InChunkQueue)
{
	VoxelModule = InVoxelModule;
	ChunkQueue = InChunkQueue;
}

void AsyncTask_LoadChunkMap::DoWork()
{
	for(auto Iter : ChunkQueue)
	{
		VoxelModule->LoadChunkMap(Iter);
	}
}
