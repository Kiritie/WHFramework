// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/AsyncTasks/ChunkMapBuildTask.h"

#include "Global/GlobalBPLibrary.h"
#include "Voxel/VoxelModule.h"

ChunkMapBuildTask::ChunkMapBuildTask(int32 InTaskIndex, class AVoxelModule* InVoxelModule, TArray<AVoxelChunk*> InChunkMapBuildQueue)
{
	TaskIndex = InTaskIndex;
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
	// if(VoxelModule->ChunkMapBuildTasks.IsValidIndex(TaskIndex))
	// {
	// 	VoxelModule->ChunkMapBuildTasks[TaskIndex] = nullptr;
	// }
	// bool bIsAllChunkBuilded = true;
	// for(auto Iter : VoxelModule->ChunkMapBuildTasks)
	// {
	// 	if(Iter)
	// 	{
	// 		bIsAllChunkBuilded = false;
	// 		break;
	// 	}
	// }
	// if(bIsAllChunkBuilded)
	// {
	// 	VoxelModule->ChunkMapBuildTasks.Empty();
	// }
}

bool ChunkMapBuildTask::CanWork() const
{
	return VoxelModule && VoxelModule->IsValidLowLevel() && UGlobalBPLibrary::IsPlaying();
}
