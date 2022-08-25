// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Math/MathTypes.h"

class AVoxelModule;
/**
 * 
 */
class AsyncTask_BuildChunkMesh : public FNonAbandonableTask
{
	friend class FAsyncTask<AsyncTask_BuildChunkMesh>;

public:
	AsyncTask_BuildChunkMesh(AVoxelModule* InVoxelModule, TArray<FIndex> InChunkQueue);

protected:
	AVoxelModule* VoxelModule;
	TArray<FIndex> ChunkQueue;

public:
	void DoWork();

public:
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(ChunkMapBuildTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};