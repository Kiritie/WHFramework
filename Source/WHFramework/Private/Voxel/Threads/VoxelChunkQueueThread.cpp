

#include "Voxel/Threads/VoxelChunkQueueThread.h"

FVoxelChunkQueueThread::FVoxelChunkQueueThread(const TArray<FIndex>& InQueue, const TFunction<void(FIndex, int32)>& InFunc, int32 InStage)
{
	ChunkQueue = InQueue;
	Func = InFunc;
	Stage = InStage;

	bFinished.Store(false);
	bStopRequested.Store(false);

	Thread = FRunnableThread::Create(this, TEXT("VoxelChunkQueueThread"));
}

FVoxelChunkQueueThread::~FVoxelChunkQueueThread()
{
	if(Thread)
	{
		Stop();
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

uint32 FVoxelChunkQueueThread::Run()
{
	for(const auto& Iter : ChunkQueue)
	{
		if(bStopRequested.Load()) break;
		Func(Iter, Stage);
	}
	bFinished.Store(true);
	return 0;
}

void FVoxelChunkQueueThread::Stop()
{
	bStopRequested.Store(true);
}
