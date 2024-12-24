// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelModuleTypes.h"
#include "Chunks/VoxelChunk.h"
#include "Common/CommonTypes.h"
#include "Main/Base/ModuleBase.h"

#include "VoxelModule.generated.h"

const int32 LoadRadius = 5;
const int32 ChunkSize = LoadRadius * 2 - 1;
const int32 Center1 = LoadRadius - 1;

const int32 DisplayRadius = LoadRadius-1;
const int32 DisplaySize = DisplayRadius * 2- 1;
const int32 DisplayCenter = DisplayRadius - 1;

const int32 SeaLevel = 0;

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
	
	GENERATED_MODULE(UVoxelModule)

public:	
	UVoxelModule();

	~UVoxelModule();

	//////////////////////////////////////////////////////////////////////////
	/// ModuleBase
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

public:
	virtual void Load_Implementation() override;

	virtual void Save_Implementation() override;

public:
	virtual FString GetModuleDebugMessage() override;

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
	virtual AVoxelChunk* SpawnChunk(FIndex InIndex, bool bAddToQueue = true);

	virtual void GenerateChunk(FIndex InIndex);

	virtual void DestroyChunk(FIndex InIndex);

	virtual void LoadChunkMap(FIndex InIndex);

	virtual void BuildChunkMap(FIndex InIndex, int32 InStage);

	virtual void SpawnChunkMesh(FIndex InIndex);

	virtual void BuildChunkMesh(FIndex InIndex);
	
public:
	virtual void GenerateChunkQueues(bool bFromAgent = true, bool bForce = false);

	virtual void DestroyChunkQueues();

protected:
	virtual bool UpdateChunkQueue(EVoxelWorldState InState, TFunction<void(FIndex, int32)> InFunc);

	virtual void AddToChunkQueue(EVoxelWorldState InState, FIndex InIndex);
	
	virtual void RemoveFromChunkQueue(EVoxelWorldState InState, FIndex InIndex);

public:
	virtual bool IsOnTheWorld(FIndex InIndex, bool bIgnoreZ = true) const;

	virtual AVoxelChunk* GetChunkByIndex(FIndex InIndex) const;

	virtual AVoxelChunk* GetChunkByLocation(FVector InLocation) const;

	virtual AVoxelChunk* GetChunkByVoxelIndex(FIndex InIndex) const;
		
	virtual bool HasVoxelByIndex(FIndex InIndex);

	virtual bool HasVoxelByLocation(FVector InLocation);

	virtual FVoxelItem& GetVoxelByIndex(FIndex InIndex);

	virtual FVoxelItem& GetVoxelByLocation(FVector InLocation);
	
	virtual void SetVoxelByIndex(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bSafe = false);
	
	virtual void SetVoxelByLocation(FVector InLocation, const FVoxelItem& InVoxelItem, bool bSafe = false);

public:
	virtual FVoxelTopography& GetTopographyByIndex(FIndex InIndex);

	virtual FVoxelTopography& GetTopographyByLocation(FVector InLocation);

	virtual void SetTopographyByIndex(FIndex InIndex, const FVoxelTopography& InTopography);

	virtual void SetTopographyLocation(FVector InLocation, const FVoxelTopography& InTopography);

public:
	virtual float GetNoise2D(FVector2D InLocation, FVector InScale, int32 InOffset = 0, bool bAbs = false) const;

	virtual float GetNoise3D(FVector InLocation, FVector InScale, int32 InOffset = 0, bool bAbs = false) const;

	virtual float GetNoiseHeight(FVector2D InLocation, FVector InScale, float InBaseHeight) const;

	virtual float GetNoiseHeight(float InBaseHeight) const;

	virtual int32 GetTerrainHeight(FIndex InIndex) const;

	virtual EVoxelTerrainType GetTerrainType(FIndex InIndex) const;

	virtual EVoxelTerrainType GetTerrainType(int32 InX, int32 InY, int32 InZ) const;

	virtual EVoxelType GetTerrainVoxelType(FIndex InIndex) const;

	virtual EVoxelType GetTerrainVoxelType(int32 InX, int32 InY, int32 InZ) const;

	virtual EVoxelType CaculateBlockID(FIndex InIndex);

	virtual EVoxelType GetFoliageVoxelType(FIndex InIndex) const;

	virtual EVoxelType GetFoliageVoxelType(int32 InX, int32 InY, int32 InZ) const;

public:
	virtual FIndex LocationToChunkIndex(FVector InLocation, bool bIgnoreZ = false) const;

	virtual FVector ChunkIndexToLocation(FIndex InIndex) const;

	virtual FIndex ChunkIndexToVoxelIndex(FIndex InIndex) const;

	virtual FIndex LocationToVoxelIndex(FVector InLocation, bool bIgnoreZ = false) const;

	virtual FVector VoxelIndexToLocation(FIndex InIndex) const;

	virtual FIndex VoxelIndexToChunkIndex(FIndex InIndex) const;

	virtual int32 VoxelIndexToNumber(FIndex InIndex) const;

	virtual FIndex NumberToVoxelIndex(int32 InNumber) const;

public:
	virtual bool VoxelRaycastSinge(FVector InRayStart, FVector InRayEnd, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult);

	virtual bool VoxelRaycastSinge(EVoxelRaycastType InRaycastType, float InDistance, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult);

	virtual bool VoxelItemTraceSingle(const FVoxelItem& InVoxelItem, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult);

	virtual bool VoxelAgentTraceSingle(FIndex InChunkIndex, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock = false, int32 InMaxCount = 1, bool bFromCenter = false, bool bForce = false);

	virtual bool VoxelAgentTraceSingle(FVector InLocation, FVector2D InRange, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock = false, int32 InMaxCount = 1, bool bFromCenter = false, bool bForce = false);

	virtual bool VoxelAgentTraceSingle(FVector InRayStart, FVector InRayEnd, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bCheckVoxel = false);

protected:
	UPROPERTY(EditAnywhere, Category = "Chunk")
	TSubclassOf<AVoxelChunk> ChunkSpawnClass;

	UPROPERTY(EditAnywhere, Category = "Chunk")
	float ChunkSpawnDistance;

	UPROPERTY(EditAnywhere, Category = "Chunk")
	TMap<EVoxelWorldState, FVoxelChunkQueues> ChunkQueues;
	
	UPROPERTY(EditAnywhere, Category = "Voxel")
	TArray<TSubclassOf<UVoxel>> VoxelClasses;

	UPROPERTY(VisibleAnywhere)
	int32 ChunkSpawnBatch;

	UPROPERTY(VisibleAnywhere)
	FIndex ChunkGenerateIndex;

	UPROPERTY(Transient)
	TMap<FIndex, AVoxelChunk*> ChunkMap;

	UPROPERTY(Transient)
	TArray<AVoxelEntityPreview*> PreviewVoxels;

public:
	bool IsBasicGenerated() const;
	
	FBox GetWorldBounds(float InRadius = 0.f, float InHalfHeight = 0.f) const;

	int32 GetChunkNum(bool bNeedGenerated = false) const;

	bool IsChunkGenerated(FIndex InIndex, bool bCheckVerticals = false) const;
	
	TArray<AVoxelChunk*> GetVerticalChunks(FIndex InIndex) const;
	
protected:
	//生成建筑方块
	void GenerateBuildingBlocks();

protected:
	//创建Building Actor
	bool CreateBuilding(int32 id,int32 rotate, FVector pos);

public:
	//待显示建筑列表
	TArray<TTuple<uint64,int32,int32>> Budildings2Display;

public:
	//添加建筑
	void AddBuilding(FVector pos,int32 BuildingID,int32 rotate);

	//待显示建筑列表
	TArray<TTuple<uint64,int32,int32>>& GetBuildings2Display();
};
