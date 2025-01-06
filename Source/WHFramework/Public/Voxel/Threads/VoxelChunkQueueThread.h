// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Math/MathTypes.h"

class FVoxelChunkQueueThread : public FRunnable
{
public:
	FVoxelChunkQueueThread(const TArray<FIndex>& InQueue, const TFunction<void(FIndex, int32)>& InFunc, int32 InStage);

	virtual ~FVoxelChunkQueueThread() override;

public:
	virtual bool Init() override;
	
	virtual uint32 Run() override;
	
	virtual void Stop()override;
	
	virtual void Exit() override;
	
protected:
	TArray<FIndex> ChunkQueue;
	TFunction<void(FIndex, int32)> Func;
	int32 Stage;
	bool bFinished;
	FRunnableThread* Thread;

public:
	TArray<FIndex>& GetChunkQueue() { return ChunkQueue; }

	bool IsFinished() const { return bFinished; }
};
