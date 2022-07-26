// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/AsyncTasks/ChunkMapBuildTask.h"

#include "Global/GlobalBPLibrary.h"
#include "Voxel/VoxelModule.h"

ChunkMapBuildTask::ChunkMapBuildTask(AVoxelModule* InVoxelModule, TArray<FIndex> InChunkMapBuildQueue)
{
	VoxelModule = InVoxelModule;
	ChunkMapBuildQueue = InChunkMapBuildQueue;
}

void ChunkMapBuildTask::DoWork()
{
	for(auto Iter : ChunkMapBuildQueue)
	{
		if(!CanWork()) return;
		VoxelModule->BuildChunkMap(Iter, 0);
	}
}

bool ChunkMapBuildTask::CanWork() const
{
	return VoxelModule && VoxelModule->IsValidLowLevel() && UGlobalBPLibrary::IsPlaying();
}
