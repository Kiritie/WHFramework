// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelModuleTypes.h"
#include "GameFramework/Actor.h"
#include "Common/CommonTypes.h"
#include "Main/Base/ModuleBase.h"
#include "VoxelModule.generated.h"

class AVoxelCapture;
class AVoxelChunk;
class UVoxelData;
class ACharacterBase;
class UWorldTimer;
class UWorldWeather;
class AVoxelEntityPreview;

/**
 * 体素模块
 */
UCLASS()
class WHFRAMEWORK_API UVoxelModule : public UModuleBase
{
	GENERATED_BODY()

	friend class AVoxelChunk;
	friend class AsyncTask_BuildChunkMap;
	friend class AsyncTask_BuildChunkMesh;
	friend class AsyncTask_LoadChunkMap;
	friend class AsyncTask_SaveChunkData;
		
	GENERATED_MODULE(UVoxelModule)

public:	
	UVoxelModule();

	~UVoxelModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

public:
	virtual void Load_Implementation() override;

	virtual void Save_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	// Capture
protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Capture")
	AVoxelCapture* VoxelCapture;

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

	virtual FVoxelWorldSaveData* NewWorldData(FSaveData* InBasicData = nullptr) const;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* GetData() override { return WorldData; }

	virtual FSaveData* ToData() override;

	virtual void UnloadData(EPhase InPhase) override;

protected:
	virtual void GenerateWorld();
	
public:
	virtual void LoadChunkMap(FIndex InIndex);

	virtual void BuildChunkMap(FIndex InIndex, int32 InStage);

	virtual void BuildChunkMesh(FIndex InIndex);
	
	virtual AVoxelChunk* SpawnChunk(FIndex InIndex, bool bAddToQueue = true);

	virtual void GenerateChunk(FIndex InIndex);

	virtual void DestroyChunk(FIndex InIndex);

protected:
	virtual void AddToChunkQueue(EVoxelWorldState InState, FIndex InIndex);
	
	virtual void RemoveFromChunkQueue(EVoxelWorldState InState, FIndex InIndex);

public:
	virtual bool IsOnTheWorld(FIndex InIndex, bool bIgnoreZ = true) const;

public:
	virtual AVoxelChunk* FindChunkByIndex(FIndex InIndex) const;

	virtual AVoxelChunk* FindChunkByLocation(FVector InLocation) const;
	
	virtual FVoxelItem& FindVoxelByIndex(FIndex InIndex);

	virtual FVoxelItem& FindVoxelByLocation(FVector InLocation);

public:
	virtual float GetNoiseHeight(FVector2D InLocation, FVector InScale, int32 InOffset) const;

	virtual float GetNoiseHeight(float InBaseHeight) const;

	virtual EVoxelType GetNoiseVoxelType(FIndex InIndex) const;

	virtual EVoxelType GetNoiseVoxelType(int32 InX, int32 InY, int32 InZ) const;

	virtual EVoxelType GetRandomVoxelType(FIndex InIndex) const;

	virtual EVoxelType GetRandomVoxelType(int32 InX, int32 InY, int32 InZ) const;

	virtual FIndex LocationToChunkIndex(FVector InLocation, bool bIgnoreZ = false) const;

	virtual FVector ChunkIndexToLocation(FIndex InIndex) const;

	virtual FIndex LocationToVoxelIndex(FVector InLocation, bool bIgnoreZ = false) const;

	virtual FVector VoxelIndexToLocation(FIndex InIndex) const;

	virtual int32 VoxelIndexToNumber(FIndex InIndex) const;

	virtual FIndex NumberToVoxelIndex(int32 InNumber) const;

public:
	virtual ECollisionChannel GetChunkTraceChannel() const;
	
	virtual ECollisionChannel GetVoxelTraceChannel() const;

	virtual bool VoxelRaycastSinge(FVector InRayStart, FVector InRayEnd, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult);

	virtual bool VoxelRaycastSinge(EVoxelRaycastType InRaycastType, float InDistance, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult);

	virtual bool VoxelItemTraceSingle(const FVoxelItem& InVoxelItem, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult);

	virtual bool VoxelAgentTraceSingle(FIndex InChunkIndex, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock = false, int32 InMaxCount = 1, bool bFromCenter = false);

	virtual bool VoxelAgentTraceSingle(FVector InLocation, FVector2D InRange, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock = false, int32 InMaxCount = 1, bool bFromCenter = false);

	virtual bool VoxelAgentTraceSingle(FVector InRayStart, FVector InRayEnd, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult);

public:
	void GenerateChunkQueues(bool bFromAgent = true, bool bForce = false);

	void DestroyChunkQueues();

protected:
	bool UpdateChunkQueue(EVoxelWorldState InState, TFunction<void(FIndex, int32)> InFunc);

protected:
	UPROPERTY(EditAnywhere, Category = "Chunk")
	TSubclassOf<AVoxelChunk> ChunkSpawnClass;

	UPROPERTY(EditAnywhere, Category = "Chunk")
	float ChunkSpawnDistance;

	UPROPERTY(EditAnywhere, Category = "Chunk")
	TMap<EVoxelWorldState, FVoxelChunkQueues> ChunkQueues;
	
	UPROPERTY(EditAnywhere, Category = "Voxel")
	TArray<TSubclassOf<UVoxel>> VoxelClasses;

	TMap<FIndex, AVoxelChunk*> ChunkMap;

	TArray<AVoxelEntityPreview*> PreviewVoxels;

protected:
	int32 ChunkSpawnBatch;

	FIndex ChunkGenerateIndex;

public:
	bool IsBasicGenerated() const;
	
	FBox GetWorldBounds(float InRadius = 0.f, float InHalfHeight = 0.f) const;

	int32 GetChunkNum(bool bNeedGenerated = false) const;

	bool IsChunkGenerated(FIndex InIndex, bool bCheckVerticals = false) const;
	
	TArray<AVoxelChunk*> GetVerticalChunks(FIndex InIndex) const;
};
