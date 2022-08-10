// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/AsyncTasks/AsyncTask_BuildChunkMesh.h"

#include "Global/GlobalBPLibrary.h"
#include "Voxel/VoxelModule.h"

AsyncTask_BuildChunkMesh::AsyncTask_BuildChunkMesh(AVoxelModule* InVoxelModule, TArray<FIndex> InChunkQueue)
{
	VoxelModule = InVoxelModule;
	ChunkQueue = InChunkQueue;
}

void AsyncTask_BuildChunkMesh::DoWork()
{
	for(auto Iter : ChunkQueue)
	{
		if(!CanWork()) return;
		VoxelModule->BuildChunkMesh(Iter);
	}
}

bool AsyncTask_BuildChunkMesh::CanWork() const
{
	return VoxelModule && VoxelModule->IsValidLowLevel() && UGlobalBPLibrary::IsPlaying();
}
