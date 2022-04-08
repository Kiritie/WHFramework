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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorldGenerated, FVector, InPlayerLocation, bool, bPreview);

/**
 * 体素模块
 */
UCLASS(hidecategories = (Tick, Replication, Rendering, Collision, Actor, Input, LOD, Cooking, Hidden, WorldPartition, Hlod))
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
	// Static
protected:
	static AVoxelModule* Current;
public:
	static AVoxelModule* Get();

protected:
	static FWorldSaveData* WorldData;
public:
	template<class T>
	static T* GetWorldData()
	{
		return static_cast<T*>(GetWorldData());
	}
	static FWorldSaveData* GetWorldData();

public:
	static FIndex LocationToChunkIndex(FVector InLocation, bool bIgnoreZ = false);

	static FVector ChunkIndexToLocation(FIndex InIndex);

	//////////////////////////////////////////////////////////////////////////
	// Components
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
	USceneCaptureComponent2D* VoxelsCapture;

	//////////////////////////////////////////////////////////////////////////
	// World
protected:
	UPROPERTY(EditAnywhere, Category = "World")
	int32 ChunkSpawnRange;

	UPROPERTY(EditAnywhere, Category = "World")
	int32 ChunkSpawnDistance;

	UPROPERTY(EditAnywhere, Category = "World")
	int32 ChunkSpawnSpeed;
		
	UPROPERTY(EditAnywhere, Category = "World")
	int32 ChunkDestroySpeed;
					
	UPROPERTY(EditAnywhere, Category = "World")
	int32 ChunkMapBuildSpeed;
					
	UPROPERTY(EditAnywhere, Category = "World")
	int32 ChunkMapGenerateSpeed;

	UPROPERTY(EditAnywhere, Category = "World")
	int32 ChunkGenerateSpeed;

	UPROPERTY(EditAnywhere, Category = "World")
	float BasicPercentage;

	UPROPERTY(VisibleAnywhere, Category = "World")
	bool bBasicGenerated;

	FRandomStream RandomStream;

public:
	UPROPERTY(BlueprintAssignable)
	FOnWorldGenerated OnWorldGenerated;

public:
	virtual void LoadData(FSaveData* InWorldData) override;

	virtual FSaveData* ToData(bool bSaved = true) override;

	virtual void UnloadData(bool bPreview = false);

public:
	UFUNCTION(BlueprintPure)
	bool IsBasicGenerated() const { return bBasicGenerated; }
	
	UFUNCTION(BlueprintPure)
	FRandomStream GetRandomStream() const { return RandomStream; }
			
	UFUNCTION(BlueprintCallable)
	void InitRandomStream(int32 InDeltaSeed);

	virtual EVoxelType GetNoiseVoxelType(FIndex InIndex);

	virtual UVoxelData* GetNoiseVoxelData(FIndex InIndex);

	virtual int GetNoiseTerrainHeight(FVector InOffset, FVector InScale);

	//////////////////////////////////////////////////////////////////////////
	// Chunk
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
	TMap<FIndex, AVoxelChunk*> ChunkMap;
	
	UPROPERTY()
	TArray<FIndex> ChunkSpawnQueue;

	UPROPERTY()
	TArray<AVoxelChunk*> ChunkMapBuildQueue;

	UPROPERTY()
	TArray<AVoxelChunk*> ChunkMapGenerateQueue;

	UPROPERTY()
	TArray<AVoxelChunk*> ChunkGenerateQueue;

	UPROPERTY()
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

public:
	int GetChunkNum(bool bNeedGenerated = false) const;

	float GetWorldLength() const;

	int32 GetChunkDistance() const;

	virtual bool ChunkTraceSingle(AVoxelChunk* InChunk, float InRadius, float InHalfHeight, FHitResult& OutHitResult);

	virtual bool ChunkTraceSingle(FVector RayStart, FVector RayEnd, float InRadius, float InHalfHeight, FHitResult& OutHitResult);

	virtual bool VoxelTraceSingle(const FVoxelItem& InVoxelItem, FVector InPoint, FHitResult& OutHitResult);
};
