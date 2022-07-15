// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelModuleTypes.h"
#include "AsyncTasks/ChunkMapBuildTask.h"
#include "AsyncTasks/ChunkMapGenerateTask.h"
#include "GameFramework/Actor.h"
#include "Main/Base/ModuleBase.h"
#include "SaveGame/Base/SaveDataInterface.h"
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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World")
	EVoxelWorldMode WorldMode;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World")
	EVoxelWorldState WorldState;
public:
	EVoxelWorldMode GetWorldMode() const { return WorldMode; }

	virtual void SetWorldMode(EVoxelWorldMode InWorldMode);

	EVoxelWorldState GetWorldState() const { return WorldState; }

	virtual float GetWorldLength() const;

protected:
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
	virtual void LoadData(FSaveData* InSaveData) override;

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
	int32 ChunkMapBuildSpeed;
					
	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkMapGenerateSpeed;

	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkGenerateSpeed;

protected:
	UPROPERTY(Transient)
	TMap<FIndex, AVoxelChunk*> ChunkMap;
	
	UPROPERTY(Transient)
	TArray<FIndex> ChunkSpawnQueue;

	UPROPERTY(Transient)
	TArray<AVoxelChunk*> ChunkMapBuildQueue;

	UPROPERTY(Transient)
	TArray<AVoxelChunk*> ChunkMapGenerateQueue;

	UPROPERTY(Transient)
	TArray<AVoxelChunk*> ChunkGenerateQueue;

	UPROPERTY(Transient)
	TArray<AVoxelChunk*> ChunkDestroyQueue;

private:
	int32 ChunkSpawnBatch;

	FIndex LastGenerateIndex;

	FIndex LastStayChunkIndex;
	
	TArray<FAsyncTask<ChunkMapBuildTask>*> ChunkMapBuildTasks;
	
	TArray<FAsyncTask<ChunkMapGenerateTask>*> ChunkMapGenerateTasks;

protected:
	virtual void GeneratePreviews();

	virtual void GenerateTerrain();

	virtual void GenerateChunks(FIndex InIndex);

public:
	virtual void BuildChunkMap(AVoxelChunk* InChunk);

	virtual void GenerateChunkMap(AVoxelChunk* InChunk);

	virtual void GenerateChunk(AVoxelChunk* InChunk);

	virtual void DestroyChunk(AVoxelChunk* InChunk);

	virtual void AddToSpawnQueue(FIndex InIndex);

	virtual void AddToMapBuildQueue(AVoxelChunk* InChunk);

	virtual void AddToMapGenerateQueue(AVoxelChunk* InChunk);

	virtual void AddToGenerateQueue(AVoxelChunk* InChunk);

	virtual void AddToDestroyQueue(AVoxelChunk* InChunk);

public:
	virtual AVoxelChunk* SpawnChunk(FIndex InIndex, bool bAddToQueue = true);

	virtual AVoxelChunk* FindChunk(FVector InLocation);

	virtual AVoxelChunk* FindChunk(FIndex InIndex);

	//////////////////////////////////////////////////////////////////////////
	// Noise
public:
	virtual EVoxelType GetNoiseVoxelType(FIndex InIndex);

	virtual UVoxelData& GetNoiseVoxelData(FIndex InIndex);

	virtual int GetNoiseTerrainHeight(FVector InOffset, FVector InScale);

	//////////////////////////////////////////////////////////////////////////
	// Trace
public:
	virtual bool ChunkTraceSingle(AVoxelChunk* InChunk, float InRadius, float InHalfHeight, FHitResult& OutHitResult);

	virtual bool ChunkTraceSingle(FVector RayStart, FVector RayEnd, float InRadius, float InHalfHeight, FHitResult& OutHitResult);

	virtual bool VoxelTraceSingle(const FVoxelItem& InVoxelItem, FVector InPoint, FHitResult& OutHitResult);

public:
	int GetChunkNum(bool bNeedGenerated = false) const;
};
