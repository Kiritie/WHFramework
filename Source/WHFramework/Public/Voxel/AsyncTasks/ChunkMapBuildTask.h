// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class AVoxelChunk;
class AVoxelModule;
/**
 * 
 */
class ChunkMapBuildTask : public FNonAbandonableTask
{
	friend class FAsyncTask<ChunkMapBuildTask>;

public:
	ChunkMapBuildTask(int32 InTaskIndex, class AVoxelModule* InVoxelModule, TArray<class AVoxelChunk*> InChunkMapBuildQueue);

protected:
	int32 TaskIndex;
	AVoxelModule* VoxelModule;
	TArray<AVoxelChunk*> ChunkMapBuildQueue;

public:
	void DoWork();

	bool CanWork() const;

public:
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(ChunkMapBuildTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};
