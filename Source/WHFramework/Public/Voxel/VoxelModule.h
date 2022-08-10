// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelModuleTypes.h"
#include "GameFramework/Actor.h"
#include "Main/Base/ModuleBase.h"
#include "SaveGame/Base/SaveDataInterface.h"
#include "Voxel/AsyncTasks/AsyncTask_LoadChunkMap.h"
#include "Voxel/AsyncTasks/AsyncTask_BuildChunkMap.h"
#include "Voxel/AsyncTasks/AsyncTask_BuildChunkMesh.h"
#include "VoxelModule.generated.h"

class AVoxelChunk;
class UVoxelData;
class ACharacterBase;
class UWorldTimerComponent;
class USceneCaptureComponent2D;
class UWorldWeatherComponent;

/**
 * 体素模块
 */
UCLASS()
class WHFRAMEWORK_API AVoxelModule : public AModuleBase, public ISaveDataInterface
{
	GENERATED_BODY()

	friend class AsyncTask_BuildChunkMap;
	friend class AsyncTask_BuildChunkMesh;
	friend class AsyncTask_LoadChunkMap;

public:	
	// Sets default values for this actor's properties
	AVoxelModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	// Components
protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	USceneCaptureComponent2D* VoxelsCapture;

	//////////////////////////////////////////////////////////////////////////
	// World
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	EVoxelWorldMode WorldMode;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World")
	EVoxelWorldState WorldState;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	FVoxelWorldBasicSaveData WorldBasicData;
public:
	EVoxelWorldMode GetWorldMode() const { return WorldMode; }

	EVoxelWorldState GetWorldState() const { return WorldState; }

	FVoxelWorldBasicSaveData GetWorldBasicData() const { return WorldBasicData; }
	
	float GetWorldLength() const;

	bool IsBasicGenerated() const;

protected:
	void SetWorldState(EVoxelWorldState InWorldState);

	virtual void OnWorldStateChanged();

	//////////////////////////////////////////////////////////////////////////
	// Data
protected:
	FVoxelWorldSaveData* WorldData;
public:
	template<class T>
	T& GetWorldData()
	{
		return static_cast<T&>(GetWorldData());
	}
	FVoxelWorldSaveData& GetWorldData() const;

protected:
	virtual void LoadData(FSaveData* InSaveData, bool bForceMode) override;

	virtual FSaveData* ToData() override;

	virtual void UnloadData(bool bForceMode) override;

	//////////////////////////////////////////////////////////////////////////
	// Chunk
protected:
	UPROPERTY(EditAnywhere, Category = "Chunk")
	TSubclassOf<AVoxelChunk> ChunkSpawnClass;

	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkSpawnRange;

	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkBasicSpawnRange;

	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkSpawnDistance;

	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkSpawnSpeed;
				
	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkDestroyDistance;

	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkDestroySpeed;
										
	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkMapLoadSpeed;

	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkMapBuildSpeed;
					
	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkMeshBuildSpeed;

	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkGenerateSpeed;

protected:
	int32 ChunkSpawnBatch;

	FIndex LastGenerateIndex;

	FIndex LastStayChunkIndex;
	
	TMap<FIndex, AVoxelChunk*> ChunkMap;
	
	TArray<FIndex> ChunkSpawnQueue;

	TArray<FIndex> ChunkMapLoadQueue;

	TArray<FIndex> ChunkMapBuildQueue;

	TArray<FIndex> ChunkMapBuildQueue1;

	TArray<FIndex> ChunkMeshBuildQueue;

	TArray<FIndex> ChunkGenerateQueue;

	TArray<FIndex> ChunkDestroyQueue;
		
	TArray<FAsyncTask<AsyncTask_LoadChunkMap>*> ChunkMapLoadTasks;

	TArray<FAsyncTask<AsyncTask_BuildChunkMap>*> ChunkMapBuildTasks;
	
	TArray<FAsyncTask<AsyncTask_BuildChunkMesh>*> ChunkMeshBuildTasks;

protected:
	virtual void GenerateWorld();

	virtual void GenerateVoxels();

protected:
	virtual void LoadChunkMap(FIndex InIndex);

	virtual void BuildChunkMap(FIndex InIndex, int32 InStage);

	virtual void BuildChunkMesh(FIndex InIndex);

	virtual void GenerateChunk(FIndex InIndex);

	virtual void DestroyChunk(FIndex InIndex);

	virtual bool AddToSpawnQueue(FIndex InIndex);

	virtual bool RemoveFromSpawnQueue(FIndex InIndex);

	virtual bool AddToMapLoadQueue(FIndex InIndex);

	virtual bool RemoveFromMapLoadQueue(FIndex InIndex);

	virtual bool AddToMapBuildQueue(FIndex InIndex, int32 InStage);

	virtual bool RemoveFromMapBuildQueue(FIndex InIndex, int32 InStage);

	virtual bool AddToMeshBuildQueue(FIndex InIndex);

	virtual bool RemoveFromMeshBuildQueue(FIndex InIndex);

	virtual bool AddToGenerateQueue(FIndex InIndex);

	virtual bool RemoveFromGenerateQueue(FIndex InIndex);

	virtual bool AddToDestroyQueue(FIndex InIndex);
	
	virtual bool RemoveFromDestroyQueue(FIndex InIndex);

public:
	virtual AVoxelChunk* SpawnChunk(FIndex InIndex, bool bAddToQueue = true);

	virtual AVoxelChunk* FindChunkByIndex(FIndex InIndex);

	virtual AVoxelChunk* FindChunkByLocation(FVector InLocation);

protected:
	bool UpdateChunkQueue(TArray<FIndex>& InQueue, int32 InSpeed, TFunction<void(FIndex)> InFunc)
	{
		int32 index = 0;
		while(index < InSpeed && InQueue.Num() > 0)
		{
			InFunc(InQueue[0]);
			index++;
		}
		return InQueue.Num() > 0;
	}
	
	bool UpdateChunkQueue(TArray<FIndex>& InQueue, int32 InSpeed, TFunction<void(FIndex, int32)> InFunc, int32 InStage)
	{
		int32 index = 0;
		while(index < InSpeed && InQueue.Num() > 0)
		{
			InFunc(InQueue[0], InStage);
			index++;
		}
		return InQueue.Num() > 0;
	}
	
	template<class T>
	bool UpdateChunkTasks(TArray<FIndex>& InQueue, TArray<FAsyncTask<T>*>& InTasks, int32 InSpeed)
	{
		if(InTasks.Num() < FMath::CeilToInt((float)InQueue.Num() / InSpeed))
		{
			const auto tmpTask = new FAsyncTask<T>(this, InQueue);
			InTasks.Add(tmpTask);
			tmpTask->StartBackgroundTask();
		}
		TArray<FAsyncTask<T>*> tmpTask;
		for(auto iter : InTasks)
		{
			if(iter->IsDone())
			{
				tmpTask.Add(iter);
			}
		}
		for(auto iter : tmpTask)
		{
			InTasks.Remove(iter);
			delete iter;
		}
		return InQueue.Num() > 0 || InTasks.Num() > 0;
	}

	template<class T>
	void StopChunkTasks(TArray<FAsyncTask<T>*>& InTasks)
	{
		for(auto iter : InTasks)
		{
			iter->Cancel();
			// delete iter;
		}
		InTasks.Empty();
	}

	//////////////////////////////////////////////////////////////////////////
	// Noise
public:
	virtual EVoxelType GetNoiseVoxelType(FIndex InIndex);

	virtual UVoxelData& GetNoiseVoxelData(FIndex InIndex);

	virtual int32 GetNoiseTerrainHeight(FVector InOffset, FVector InScale);

	//////////////////////////////////////////////////////////////////////////
	// Trace
public:
	virtual bool ChunkTraceSingle(AVoxelChunk* InChunk, float InRadius, float InHalfHeight, FHitResult& OutHitResult);

	virtual bool ChunkTraceSingle(FVector RayStart, FVector RayEnd, float InRadius, float InHalfHeight, FHitResult& OutHitResult);

	virtual bool VoxelTraceSingle(const FVoxelItem& InVoxelItem, FVector InPoint, FHitResult& OutHitResult);

public:
	int GetChunkNum(bool bNeedGenerated = false) const;
};
