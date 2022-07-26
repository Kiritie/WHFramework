// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/AsyncTasks/ChunkMapGenerateTask.h"

#include "Global/GlobalBPLibrary.h"
#include "Voxel/VoxelModule.h"

ChunkMapGenerateTask::ChunkMapGenerateTask(AVoxelModule* InVoxelModule, TArray<FIndex> InChunkMapGenerateQueue)
{
	VoxelModule = InVoxelModule;
	ChunkMapGenerateQueue = InChunkMapGenerateQueue;
}

void ChunkMapGenerateTask::DoWork()
{
	for(auto Iter : ChunkMapGenerateQueue)
	{
		if(!CanWork()) return;
		VoxelModule->GenerateChunkMap(Iter);
	}
}

bool ChunkMapGenerateTask::CanWork() const
{
	return VoxelModule && VoxelModule->IsValidLowLevel() && UGlobalBPLibrary::IsPlaying();
}
