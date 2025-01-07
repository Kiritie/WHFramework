// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Threads/VoxelChunkQueueThread.h"

FVoxelChunkQueueThread::FVoxelChunkQueueThread(const TArray<FIndex>& InQueue, const TFunction<void(FIndex, int32)>& InFunc, int32 InStage)
{
	ChunkQueue = InQueue;
	Func = InFunc;
	Stage = InStage;

	bFinished = false;

	Thread = FRunnableThread::Create(this, TEXT("VoxelChunkQueueThread"));
}

FVoxelChunkQueueThread::~FVoxelChunkQueueThread()
{
	if(Thread)
	{
		delete Thread;
		Thread = nullptr;
	}
}

bool FVoxelChunkQueueThread::Init()
{
	bFinished = false;
	return true;
}

uint32 FVoxelChunkQueueThread::Run()
{
	for(const auto& Iter : ChunkQueue)
	{
		Func(Iter, Stage);
	}
	bFinished = true;
	return 0;
}

void FVoxelChunkQueueThread::Stop()
{
	
}

void FVoxelChunkQueueThread::Exit()
{
	
}
