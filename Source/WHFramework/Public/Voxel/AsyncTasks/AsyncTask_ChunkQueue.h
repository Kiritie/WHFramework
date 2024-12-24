// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Math/MathTypes.h"

/**
 * 
 */
class FAsyncTask_ChunkQueue : public FNonAbandonableTask
{
	friend class FAsyncTask<FAsyncTask_ChunkQueue>;

public:
	FAsyncTask_ChunkQueue(TArray<FIndex> InQueue, TFunction<void(FIndex, int32)> InFunc, int32 InStage);

protected:
	TArray<FIndex> ChunkQueue;
	TFunction<void(FIndex, int32)> Func;
	int32 Stage;

public:
	void DoWork();

public:
	TArray<FIndex>& GetChunkQueue() { return ChunkQueue; }

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(ChunkMapBuildTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};
