#include "Voxel/Threads/VoxelChunkQueueThread.h"

#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"

FVoxelChunkQueueBatch::FVoxelChunkQueueBatch(TArray<FIndex>&& InQueue)
	: Queue(MoveTemp(InQueue)), QueueIndex(0), bCancelled(false)
{
}

bool FVoxelChunkQueueBatch::Dequeue(FIndex& OutIndex)
{
	if(bCancelled.Load()) return false;

	const int32 Index = QueueIndex++;
	if(!Queue.IsValidIndex(Index)) return false;

	OutIndex = Queue[Index];
	return !bCancelled.Load();
}

void FVoxelChunkQueueBatch::Cancel()
{
	bCancelled.Store(true);
}

FVoxelChunkQueueThread::FVoxelChunkQueueThread()
{
	Thread = nullptr;
	WorkEvent = FPlatformProcess::GetSynchEventFromPool(false);
	IdleEvent = FPlatformProcess::GetSynchEventFromPool(true);
	Stage = 0;
	bIdle.Store(true);
	bStopRequested.Store(false);
	if(IdleEvent) IdleEvent->Trigger();

	static TAtomic<int32> WorkerIndex(0);
	const FString ThreadName = FString::Printf(TEXT("VoxelChunkQueueThread_%d"), WorkerIndex++);
	Thread = FRunnableThread::Create(this, *ThreadName);
}

FVoxelChunkQueueThread::~FVoxelChunkQueueThread()
{
	Stop();
	if(Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
	if(WorkEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(WorkEvent);
		WorkEvent = nullptr;
	}
	if(IdleEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(IdleEvent);
		IdleEvent = nullptr;
	}
}

uint32 FVoxelChunkQueueThread::Run()
{
	while(!bStopRequested.Load())
	{
		WorkEvent->Wait();
		if(bStopRequested.Load()) break;

		TSharedPtr<FVoxelChunkQueueBatch, ESPMode::ThreadSafe> CurrentBatch;
		TFunction<void(FIndex, int32)> CurrentFunc;
		int32 CurrentStage = 0;
		{
			FScopeLock ScopeLock(&WorkCriticalSection);
			CurrentBatch = Batch;
			CurrentFunc = Func;
			CurrentStage = Stage;
		}

		if(CurrentBatch && CurrentFunc)
		{
			FIndex Index;
			while(!bStopRequested.Load() && CurrentBatch->Dequeue(Index))
			{
				CurrentFunc(Index, CurrentStage);
			}
		}

		{
			FScopeLock ScopeLock(&WorkCriticalSection);
			Batch.Reset();
			Func = TFunction<void(FIndex, int32)>();
			Stage = 0;
			bIdle.Store(true);
			IdleEvent->Trigger();
		}
	}

	bIdle.Store(true);
	IdleEvent->Trigger();
	return 0;
}

void FVoxelChunkQueueThread::Stop()
{
	bStopRequested.Store(true);
	{
		FScopeLock ScopeLock(&WorkCriticalSection);
		if(Batch) Batch->Cancel();
	}
	if(WorkEvent) WorkEvent->Trigger();
}

bool FVoxelChunkQueueThread::Dispatch(const TSharedRef<FVoxelChunkQueueBatch, ESPMode::ThreadSafe>& InBatch, const TFunction<void(FIndex, int32)>& InFunc, int32 InStage)
{
	if(!Thread || bStopRequested.Load() || !bIdle.Load()) return false;

	{
		FScopeLock ScopeLock(&WorkCriticalSection);
		if(bStopRequested.Load() || !bIdle.Load()) return false;
		Batch = InBatch;
		Func = InFunc;
		Stage = InStage;
		bIdle.Store(false);
		IdleEvent->Reset();
	}
	WorkEvent->Trigger();
	return true;
}

void FVoxelChunkQueueThread::WaitForIdle() const
{
	if(IdleEvent && !bIdle.Load()) IdleEvent->Wait();
}
