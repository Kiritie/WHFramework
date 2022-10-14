// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/AsyncTasks/AsyncTask_SaveChunkData.h"

#include "Global/GlobalBPLibrary.h"
#include "Voxel/VoxelModule.h"

AsyncTask_SaveChunkData::AsyncTask_SaveChunkData(AVoxelModule* InVoxelModule, TArray<FIndex> InChunkQueue)
{
	VoxelModule = InVoxelModule;
	ChunkQueue = InChunkQueue;
}

void AsyncTask_SaveChunkData::DoWork()
{
	for(auto Iter : ChunkQueue)
	{
		VoxelModule->SaveChunkData(Iter);
	}
}
