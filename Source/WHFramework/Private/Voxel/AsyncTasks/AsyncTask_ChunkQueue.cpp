// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/AsyncTasks/AsyncTask_ChunkQueue.h"

#include "Common/CommonStatics.h"

FAsyncTask_ChunkQueue::FAsyncTask_ChunkQueue(TArray<FIndex> InQueue, TFunction<void(FIndex, int32)> InFunc, int32 InStage)
{
	ChunkQueue = InQueue;
	Func = InFunc;
	Stage = InStage;
}

void FAsyncTask_ChunkQueue::DoWork()
{
	for(const auto Iter : ChunkQueue)
	{
		Func(Iter, Stage);
	}
}
