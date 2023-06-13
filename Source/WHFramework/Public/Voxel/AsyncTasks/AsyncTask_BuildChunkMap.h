// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Math/MathTypes.h"

class AVoxelModule;
/**
 * 
 */
class AsyncTask_BuildChunkMap : public FNonAbandonableTask
{
	friend class FAsyncTask<AsyncTask_BuildChunkMap>;

public:
	AsyncTask_BuildChunkMap(AVoxelModule* InVoxelModule, TArray<FIndex> InChunkQueue);

protected:
	AVoxelModule* VoxelModule;
	TArray<FIndex> ChunkQueue;

public:
	void DoWork();

public:
	TArray<FIndex> GetChunkQueue() const { return ChunkQueue; }

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(ChunkMapBuildTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};
