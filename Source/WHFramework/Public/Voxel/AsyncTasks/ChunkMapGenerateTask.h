// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class AVoxelChunk;
class AVoxelModule;
/**
 * 
 */
class ChunkMapGenerateTask : public FNonAbandonableTask
{
	friend class FAsyncTask<ChunkMapGenerateTask>;

public:
	ChunkMapGenerateTask(int32 InTaskIndex, class AVoxelModule* InVoxelModule, TArray<class AVoxelChunk*> InChunkMapGenerateQueue);

protected:
	int32 TaskIndex;
	AVoxelModule* VoxelModule;
	TArray<AVoxelChunk*> ChunkMapGenerateQueue;

public:
	void DoWork();

	bool CanWork() const;

public:
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(ChunkMapGenerateTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};
