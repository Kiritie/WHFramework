// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Math/MathTypes.h"

class AVoxelModule;
/**
 * 
 */
class ChunkMapGenerateTask : public FNonAbandonableTask
{
	friend class FAsyncTask<ChunkMapGenerateTask>;

public:
	ChunkMapGenerateTask(AVoxelModule* InVoxelModule, TArray<FIndex> InChunkMapGenerateQueue);

protected:
	AVoxelModule* VoxelModule;
	TArray<FIndex> ChunkMapGenerateQueue;

public:
	void DoWork();

	bool CanWork() const;

public:
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(ChunkMapGenerateTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};
