// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/AsyncTasks/ChunkMapGenerateTask.h"

#include "Global/GlobalBPLibrary.h"
#include "Voxel/VoxelModule.h"

ChunkMapGenerateTask::ChunkMapGenerateTask(int32 InTaskIndex, class AVoxelModule* InVoxelModule, TArray<AVoxelChunk*> InChunkMapGenerateQueue)
{
	TaskIndex = InTaskIndex;
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
	// bool bIsAllChunkGenerateed = true;
	// for(auto Iter : VoxelModule->ChunkMapGenerateTasks)
	// {
	// 	if(&Iter->GetTask() != this && !Iter->IsDone())
	// 	{
	// 		bIsAllChunkGenerateed = false;
	// 		break;
	// 	}
	// }
	// if(bIsAllChunkGenerateed)
	// {
	// 	VoxelModule->ChunkMapGenerateTasks.Empty();
	// }
}

bool ChunkMapGenerateTask::CanWork() const
{
	return VoxelModule && VoxelModule->IsValidLowLevel() && UGlobalBPLibrary::IsPlaying();
}
