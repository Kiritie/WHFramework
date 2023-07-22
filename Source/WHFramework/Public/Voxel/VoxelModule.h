// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelModuleTypes.h"
#include "GameFramework/Actor.h"
#include "Global/GlobalTypes.h"
#include "Main/Base/ModuleBase.h"
#include "SaveGame/Base/SaveDataInterface.h"
#include "Voxel/AsyncTasks/AsyncTask_LoadChunkMap.h"
#include "Voxel/AsyncTasks/AsyncTask_BuildChunkMap.h"
#include "Voxel/AsyncTasks/AsyncTask_BuildChunkMesh.h"
#include "AsyncTasks/AsyncTask_SaveChunkData.h"
#include "VoxelModule.generated.h"

class AVoxelChunk;
class UVoxelData;
class ACharacterBase;
class UWorldTimerComponent;
class USceneCaptureComponent2D;
class UWorldWeatherComponent;
class AVoxelEntityPreview;

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
	friend class AsyncTask_SaveChunkData;
		
	GENERATED_MODULE(AVoxelModule)

public:	
	AVoxelModule();

	~AVoxelModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

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
	// Voxel
protected:
	UPROPERTY(EditAnywhere, Category = "World")
	bool bAutoGenerate;

	UPROPERTY(VisibleAnywhere, Category = "World")
	EVoxelWorldMode WorldMode;

	UPROPERTY(VisibleAnywhere, Category = "World")
	EVoxelWorldState WorldState;

	UPROPERTY(EditAnywhere, Category = "World")
	FVoxelWorldBasicSaveData WorldBasicData;
public:
	UFUNCTION(BlueprintPure)
	EVoxelWorldMode GetWorldMode() const { return WorldMode; }

	UFUNCTION(BlueprintPure)
	EVoxelWorldState GetWorldState() const { return WorldState; }

	UFUNCTION(BlueprintPure)
	FVoxelWorldBasicSaveData& GetWorldBasicData() { return WorldBasicData; }

protected:
	void SetWorldMode(EVoxelWorldMode InWorldMode);

	void SetWorldState(EVoxelWorldState InWorldState);

	virtual void OnWorldModeChanged();

	virtual void OnWorldStateChanged();

protected:
	FVoxelWorldSaveData* WorldData;
public:
	template<class T>
	T& GetWorldData() const
	{
		return static_cast<T&>(GetWorldData());
	}
	FVoxelWorldSaveData& GetWorldData() const;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase = EPhase::Final) override;

	virtual FSaveData* ToData() override;

	virtual void UnloadData(EPhase InPhase = EPhase::Final) override;

	virtual FVoxelWorldSaveData* NewData(bool bInheritBasicData = false) const;

protected:
	virtual void GenerateWorld();
	
public:
	virtual void LoadChunkMap(FIndex InIndex);

	virtual void BuildChunkMap(FIndex InIndex, int32 InStage);

	virtual void BuildChunkMesh(FIndex InIndex);
	
	virtual AVoxelChunk* SpawnChunk(FIndex InIndex, bool bAddToQueue = true);

	virtual void SaveChunkData(FIndex InIndex);

	virtual void GenerateChunk(FIndex InIndex);

	virtual void DestroyChunk(FIndex InIndex);

protected:
	virtual bool AddToSpawnQueue(FIndex InIndex);

	virtual bool RemoveFromSpawnQueue(FIndex InIndex);

	virtual bool AddToMapLoadQueue(FIndex InIndex);

	virtual bool RemoveFromMapLoadQueue(FIndex InIndex);

	virtual bool AddToMapBuildQueue(FIndex InIndex, int32 InStage);

	virtual bool RemoveFromMapBuildQueue(FIndex InIndex, int32 InStage);

	virtual bool AddToMeshBuildQueue(FIndex InIndex);

	virtual bool RemoveFromMeshBuildQueue(FIndex InIndex);
	
	virtual bool AddToDataSaveQueue(FIndex InIndex);

	virtual bool RemoveFromDataSaveQueue(FIndex InIndex);

	virtual bool AddToGenerateQueue(FIndex InIndex);

	virtual bool RemoveFromGenerateQueue(FIndex InIndex);

	virtual bool AddToDestroyQueue(FIndex InIndex);
	
	virtual bool RemoveFromDestroyQueue(FIndex InIndex);

public:
	virtual AVoxelChunk* FindChunkByIndex(FIndex InIndex) const;

	virtual AVoxelChunk* FindChunkByLocation(FVector InLocation) const;
	
	virtual FVoxelItem& FindVoxelByIndex(FIndex InIndex);

	virtual FVoxelItem& FindVoxelByLocation(FVector InLocation);

public:
	virtual EVoxelType GetNoiseVoxelType(FIndex InIndex) const;

	virtual FIndex LocationToChunkIndex(FVector InLocation, bool bIgnoreZ = false) const;

	virtual FVector ChunkIndexToLocation(FIndex InIndex) const;

	virtual FIndex LocationToVoxelIndex(FVector InLocation, bool bIgnoreZ = false) const;

	virtual FVector VoxelIndexToLocation(FIndex InIndex) const;

public:
	virtual ECollisionChannel GetChunkTraceType() const;
	
	virtual ECollisionChannel GetVoxelTraceType() const;

	virtual bool VoxelRaycastSinge(FVector InRayStart, FVector InRayEnd, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult);

	virtual bool VoxelRaycastSinge(EVoxelRaycastType InRaycastType, float InDistance, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult);

	virtual bool VoxelItemTraceSingle(const FVoxelItem& InVoxelItem, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult);

	virtual bool VoxelAgentTraceSingle(FIndex InChunkIndex, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock = false, int32 InMaxCount = 1, bool bFromCenter = false);

	virtual bool VoxelAgentTraceSingle(FVector InLocation, FVector2D InRange, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock = false, int32 InMaxCount = 1, bool bFromCenter = false);

	virtual bool VoxelAgentTraceSingle(FVector InRayStart, FVector InRayEnd, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult);

public:
	bool UpdateChunkQueues(bool bFromAgent = true, bool bForceStop = false);

	void StopChunkQueues();

protected:
	bool UpdateChunkQueue(TArray<FIndex>& InQueue, int32 InSpeed, TFunction<void(FIndex)> InFunc) const;

	bool UpdateChunkQueue(TArray<FIndex>& InQueue, int32 InSpeed, TFunction<void(FIndex, int32)> InFunc, int32 InStage) const;

	bool UpdateChunkQueue(TArray<TArray<FIndex>>& InQueue, int32 InSpeed, TFunction<bool(TArray<FIndex>& _InQueue, int32 _InSpeed, int32 _InIndex)> InFunc) const;

	template<class T>
	bool UpdateChunkQueue(TArray<FIndex>& InQueue, TArray<FAsyncTask<T>*>& InTasks, int32 InSpeed)
	{
		if(InTasks.Num() == 0 && InQueue.Num() > 0)
		{
			TArray<FIndex> Queue = InQueue;
			DON_WITHINDEX(FMath::CeilToInt((float)Queue.Num() / InSpeed), i,
				TArray<FIndex> tmpArr;
				DON_WITHINDEX(FMath::Min(InSpeed, Queue.Num() - i * InSpeed), j,
					tmpArr.Add(Queue[i * InSpeed + j]);
				)
				const auto Task = new FAsyncTask<T>(this, tmpArr);
				InTasks.Add(Task);
				Task->StartBackgroundTask();
			)
		}
		else if(InTasks.Num() > 0)
		{
			for(auto Iter = InTasks.CreateConstIterator(); Iter; ++Iter)
			{
				auto Task = *Iter;
				if(Task->IsDone())
				{
					for(auto index : Task->GetTask().GetChunkQueue())
					{
						InQueue.Remove(index);
					}
					InTasks.Remove(Task);
					delete Task;
				}
			}
		}
		return InQueue.Num() > 0 || InTasks.Num() > 0;
	}

	template<class T>
	void StopChunkTasks(TArray<FAsyncTask<T>*>& InTasks)
	{
		for(auto Iter : InTasks)
		{
			Iter->EnsureCompletion();
			delete Iter;
		}
		InTasks.Empty();
	}

protected:
	UPROPERTY(EditAnywhere, Category = "Chunk")
	TSubclassOf<AVoxelChunk> ChunkSpawnClass;
	
	UPROPERTY(EditAnywhere, Category = "Voxel")
	TArray<TSubclassOf<UVoxel>> VoxelClasses;

	UPROPERTY(EditAnywhere, Category = "Chunk")
	float ChunkSpawnDistance;

	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkSpawnSpeed;

	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkDestroySpeed;
			
	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkGenerateSpeed;
							
	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkMapLoadSpeed;

	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkMapBuildSpeed;
					
	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkMeshBuildSpeed;
	
	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkDataSaveSpeed;

	UPROPERTY(Transient)
	TMap<FIndex, AVoxelChunk*> ChunkMap;

	UPROPERTY()
	TArray<AVoxelEntityPreview*> PreviewVoxels;

protected:
	int32 ChunkSpawnBatch;

	FIndex LastGenerateIndex;

	TArray<FIndex> ChunkSpawnQueue;

	TArray<FIndex> ChunkMapLoadQueue;

	TArray<TArray<FIndex>> ChunkMapBuildQueue;

	TArray<FIndex> ChunkMeshBuildQueue;

	TArray<FIndex> ChunkGenerateQueue;
	
	TArray<FIndex> ChunkDataSaveQueue;

	TArray<FIndex> ChunkDestroyQueue;
		
	TArray<FAsyncTask<AsyncTask_LoadChunkMap>*> ChunkMapLoadTasks;

	TArray<FAsyncTask<AsyncTask_BuildChunkMap>*> ChunkMapBuildTasks;
	
	TArray<FAsyncTask<AsyncTask_BuildChunkMesh>*> ChunkMeshBuildTasks;
		
	TArray<FAsyncTask<AsyncTask_SaveChunkData>*> ChunkDataSaveTasks;

public:
	bool IsBasicGenerated() const;
	
	FBox GetWorldBounds(float InRadius = 0.f, float InHalfHeight = 0.f) const;

	int32 GetChunkNum(bool bNeedGenerated = false) const;

	bool IsChunkGenerated(FIndex InIndex, bool bCheckVerticals = false) const;
	
	TArray<AVoxelChunk*> GetVerticalChunks(FIndex InIndex) const;
};
