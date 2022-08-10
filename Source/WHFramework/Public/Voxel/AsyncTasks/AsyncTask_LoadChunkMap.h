// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Math/MathTypes.h"

class AVoxelModule;
/**
 * 
 */
class AsyncTask_LoadChunkMap : public FNonAbandonableTask
{
	friend class FAsyncTask<AsyncTask_LoadChunkMap>;

public:
	AsyncTask_LoadChunkMap(AVoxelModule* InVoxelModule, TArray<FIndex> InChunkQueue);

public:
	AVoxelModule* VoxelModule;
	TArray<FIndex> ChunkQueue;

public:
	void DoWork();

	bool CanWork() const;

public:
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(ChunkMapBuildTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};
