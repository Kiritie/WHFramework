// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/AsyncTasks/AsyncTask_ChunkQueue.h"

#include "Common/CommonStatics.h"

AsyncTask_ChunkQueue::AsyncTask_ChunkQueue(TArray<FIndex> InQueue, TFunction<void(FIndex, int32)> InFunc, int32 InStage)
{
	ChunkQueue = InQueue;
	Func = InFunc;
	Stage = InStage;
}

void AsyncTask_ChunkQueue::DoWork()
{
	for(const auto Iter : ChunkQueue)
	{
		Func(Iter, Stage);
	}
}
