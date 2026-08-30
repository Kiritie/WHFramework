

#pragma once

#include "Math/MathTypes.h"

class FVoxelChunkQueueThread : public FRunnable
{
public:
	FVoxelChunkQueueThread(const TArray<FIndex>& InQueue, const TFunction<void(FIndex, int32)>& InFunc, int32 InStage);

	virtual ~FVoxelChunkQueueThread() override;

public:
	virtual uint32 Run() override;
	
	virtual void Stop() override;
	
protected:
	TArray<FIndex> ChunkQueue;
	TFunction<void(FIndex, int32)> Func;
	int32 Stage;
	TAtomic<bool> bFinished;
	TAtomic<bool> bStopRequested;
	FRunnableThread* Thread;

public:
	const TArray<FIndex>& GetChunkQueue() const { return ChunkQueue; }

	bool IsFinished() const { return bFinished.Load(); }
};
