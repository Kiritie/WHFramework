// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Math/MathTypes.h"

class AVoxelModule;
/**
 * 
 */
class ChunkMapBuildTask : public FNonAbandonableTask
{
	friend class FAsyncTask<ChunkMapBuildTask>;

public:
	ChunkMapBuildTask(AVoxelModule* InVoxelModule, TArray<FIndex> InChunkMapBuildQueue);

protected:
	AVoxelModule* VoxelModule;
	TArray<FIndex> ChunkMapBuildQueue;

public:
	void DoWork();

	bool CanWork() const;

public:
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(ChunkMapBuildTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};
