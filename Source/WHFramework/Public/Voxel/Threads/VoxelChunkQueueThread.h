#pragma once

#include "HAL/CriticalSection.h"
#include "HAL/Runnable.h"
#include "Math/MathTypes.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

class FEvent;
class FRunnableThread;

class FVoxelChunkQueueBatch
{
public:
	explicit FVoxelChunkQueueBatch(TArray<FIndex>&& InQueue);

public:
	bool Dequeue(FIndex& OutIndex);

	void Cancel();

	const TArray<FIndex>& GetQueue() const { return Queue; }
	bool IsCancelled() const { return bCancelled.Load(); }

protected:
	TArray<FIndex> Queue;
	TAtomic<int32> QueueIndex;
	TAtomic<bool> bCancelled;
};

class FVoxelChunkQueueThread : public FRunnable
{
public:
	FVoxelChunkQueueThread();

	virtual ~FVoxelChunkQueueThread() override;

public:
	virtual uint32 Run() override;

	virtual void Stop() override;

public:
	bool Dispatch(const TSharedRef<FVoxelChunkQueueBatch, ESPMode::ThreadSafe>& InBatch, const TFunction<void(FIndex, int32)>& InFunc, int32 InStage);

	void WaitForIdle() const;

	bool IsValid() const { return Thread != nullptr; }

	bool IsIdle() const { return bIdle.Load(); }

protected:
	FRunnableThread* Thread;
	FEvent* WorkEvent;
	FEvent* IdleEvent;
	FCriticalSection WorkCriticalSection;
	TSharedPtr<FVoxelChunkQueueBatch, ESPMode::ThreadSafe> Batch;
	TFunction<void(FIndex, int32)> Func;
	int32 Stage;
	TAtomic<bool> bIdle;
	TAtomic<bool> bStopRequested;
};
