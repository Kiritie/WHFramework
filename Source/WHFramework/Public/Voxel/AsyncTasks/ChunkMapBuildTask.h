// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class ChunkMapBuildTask : public FNonAbandonableTask
{
	friend class FAsyncTask<ChunkMapBuildTask>;
	
	class AVoxelModule* VoxelModule;
	TArray<class AVoxelChunk*> ChunkMapBuildQueue;

	ChunkMapBuildTask(class AVoxelModule* InVoxelModule, TArray<class AVoxelChunk*> InChunkMapBuildQueue);

	void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(ChunkMapBuildTask, STATGROUP_ThreadPoolAsyncTasks);
	}

public:
	TArray<class AVoxelChunk*> GetQueue() const { return ChunkMapBuildQueue; }
};
