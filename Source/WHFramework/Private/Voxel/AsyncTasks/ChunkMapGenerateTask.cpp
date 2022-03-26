// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/AsyncTasks/ChunkMapGenerateTask.h"

#include "Voxel/VoxelModule.h"

ChunkMapGenerateTask::ChunkMapGenerateTask(AVoxelModule* InVoxelModule, TArray<AVoxelChunk*> InChunkMapGenerateQueue)
{
	VoxelModule = InVoxelModule;
	ChunkMapGenerateQueue = InChunkMapGenerateQueue;
}

void ChunkMapGenerateTask::DoWork()
{
	for(int32 i = 0; i < ChunkMapGenerateQueue.Num(); i++)
	{
		VoxelModule->GenerateChunkMap(ChunkMapGenerateQueue[i]);
	}
}
