// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/AsyncTasks/ChunkMapBuildTask.h"

#include "Voxel/VoxelModule.h"

ChunkMapBuildTask::ChunkMapBuildTask(AVoxelModule* InVoxelModule, TArray<AVoxelChunk*> InChunkMapBuildQueue)
{
	VoxelModule = InVoxelModule;
	ChunkMapBuildQueue = InChunkMapBuildQueue;
}

void ChunkMapBuildTask::DoWork()
{
	for(int32 i = 0; i < ChunkMapBuildQueue.Num(); i++)
	{
		VoxelModule->BuildChunkMap(ChunkMapBuildQueue[i]);
	}
}
