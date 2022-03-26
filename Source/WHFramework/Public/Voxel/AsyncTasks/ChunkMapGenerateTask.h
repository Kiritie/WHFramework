// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
* 
*/
class ChunkMapGenerateTask : public FNonAbandonableTask
{
	friend class FAsyncTask<ChunkMapGenerateTask>;

public:
	class AVoxelModule* VoxelModule;
	TArray<class AVoxelChunk*> ChunkMapGenerateQueue;

	ChunkMapGenerateTask(class AVoxelModule* InVoxelModule, TArray<class AVoxelChunk*> InChunkMapGenerateQueue);

	void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(ChunkMapGenerateTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};
