// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/AsyncTasks/AsyncTask_BuildChunkMap.h"

#include "Global/GlobalBPLibrary.h"
#include "Voxel/VoxelModule.h"

AsyncTask_BuildChunkMap::AsyncTask_BuildChunkMap(AVoxelModule* InVoxelModule, TArray<FIndex> InChunkQueue)
{
	VoxelModule = InVoxelModule;
	ChunkQueue = InChunkQueue;
}

void AsyncTask_BuildChunkMap::DoWork()
{
	for(auto Iter : ChunkQueue)
	{
		VoxelModule->BuildChunkMap(Iter, 0);
	}
}
