// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelModuleTypes.h"
#include "Chunks/VoxelChunk.h"
#include "Common/CommonTypes.h"
#include "Main/Base/ModuleBase.h"

#include "VoxelModule.generated.h"

class UVoxelGenerator;
class AVoxelCapture;
class AVoxelChunk;
class UVoxelData;
class ACharacterBase;
class UWorldTimer;
class UWorldWeather;
class AVoxelEntityCapture;

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

	UPROPERTY(Transient)
	TArray<AVoxelEntityCapture*> CaptureVoxels;

	//////////////////////////////////////////////////////////////////////////
	// World
protected:
	UPROPERTY(EditAnywhere, Category = "World")
	bool bAutoGenerate;

	UPROPERTY(EditAnywhere, Category = "World")
	EVoxelWorldMode WorldMode;

	UPROPERTY(VisibleAnywhere, Category = "World")
	EVoxelWorldState WorldState;

	UPROPERTY(EditAnywhere, Category = "World")
	FVoxelWorldBasicSaveData WorldBasicData;
	
public:
	UFUNCTION(BlueprintPure)
	EVoxelWorldMode GetWorldMode() const { return WorldMode; }

	UFUNCTION(BlueprintCallable)
	void SetWorldMode(EVoxelWorldMode InWorldMode);

	UFUNCTION(BlueprintPure)
	EVoxelWorldState GetWorldState() const { return WorldState; }

	UFUNCTION(BlueprintPure)
	FVoxelWorldBasicSaveData& GetWorldBasicData() { return WorldBasicData; }

protected:
	UFUNCTION(BlueprintCallable)
	void SetWorldState(EVoxelWorldState InWorldState);

protected:
	virtual void OnWorldModeChanged();

	virtual void OnWorldStateChanged();

	virtual void OnWorldCenterChanged();

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

	virtual float GetWorldGeneratePercent() const;
	
	virtual FBox GetWorldBounds(float InRadius = 0.f, float InHalfHeight = 0.f) const;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* GetData() override { return WorldData; }

	virtual FSaveData* ToData() override;

	virtual void UnloadData(EPhase InPhase) override;

public:
	virtual void LoadPrefabData(const FVoxelPrefabSaveData& InPrefabData);

	virtual FVoxelPrefabSaveData GetPrefabData();

protected:
	virtual void GenerateWorld();
	
public:
	virtual AVoxelChunk* SpawnChunk(FIndex InIndex, bool bAddToQueue = true);

	virtual void GenerateChunk(FIndex InIndex);

	virtual void DestroyChunk(FIndex InIndex);

	virtual void LoadChunkMap(FIndex InIndex);

	virtual void BuildChunkMap(FIndex InIndex, int32 InStage);

	virtual void SpawnChunkMesh(FIndex InIndex, int32 InStage);

	virtual void BuildChunkMesh(FIndex InIndex);
	
public:
	virtual void GenerateChunkQueues(bool bFromAgent = true, bool bForce = false);

	virtual void DestroyChunkQueues();

protected:
	virtual bool UpdateChunkQueue(EVoxelWorldState InState, TFunction<void(FIndex)> InFunc);
	
	virtual bool UpdateChunkQueue(EVoxelWorldState InState, TFunction<void(FIndex, int32)> InFunc);

	virtual void AddToChunkQueue(EVoxelWorldState InState, FIndex InIndex);
	
	virtual void RemoveFromChunkQueue(EVoxelWorldState InState, FIndex InIndex);
	
public:
	template<class T>
	bool GenerateVoxel(AVoxelChunk* InChunk) const
	{
		return GenerateVoxel(InChunk, T::StaticClass());
	}
	virtual bool GenerateVoxel(AVoxelChunk* InChunk, const TSubclassOf<UVoxelGenerator>& InClass) const;

public:
	virtual bool IsOnTheWorld(FIndex InIndex, bool bIgnoreZ = true) const;

	virtual AVoxelChunk* GetChunkByIndex(FIndex InIndex) const;

	virtual AVoxelChunk* GetChunkByLocation(FVector InLocation) const;

	virtual AVoxelChunk* GetChunkByVoxelIndex(FIndex InIndex) const;
		
	virtual bool HasVoxelByIndex(FIndex InIndex, bool bSafe = false);

	virtual bool HasVoxelByLocation(FVector InLocation, bool bSafe = false);

	virtual FVoxelItem& GetVoxelByIndex(FIndex InIndex, bool bMainPart = false);

	virtual FVoxelItem& GetVoxelByLocation(FVector InLocation, bool bMainPart = false);
	
	virtual void SetVoxelByIndex(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bSafe = false);
	
	virtual void SetVoxelByLocation(FVector InLocation, const FVoxelItem& InVoxelItem, bool bSafe = false);

public:
	virtual const FVoxelTopography& GetTopographyByIndex(FIndex InIndex);

	virtual const FVoxelTopography& GetTopographyByLocation(FVector InLocation);

	virtual void SetTopographyByIndex(FIndex InIndex, const FVoxelTopography& InTopography);

	virtual void SetTopographyByLocation(FVector InLocation, const FVoxelTopography& InTopography);

public:
	virtual float GetVoxelNoise1D(float InValue, bool bAbs = false, bool bUnsigned = false) const;

	virtual float GetVoxelNoise2D(FVector2D InLocation, bool bAbs = false, bool bUnsigned = false) const;

	virtual float GetVoxelNoise3D(FVector InLocation, bool bAbs = false, bool bUnsigned = false) const;

public:
	virtual FIndex LocationToChunkIndex(FVector InLocation) const;

	virtual FVector ChunkIndexToLocation(FIndex InIndex) const;

	virtual FIndex ChunkIndexToVoxelIndex(FIndex InIndex) const;

	virtual FIndex LocationToVoxelIndex(FVector InLocation) const;

	virtual FVector VoxelIndexToLocation(FIndex InIndex) const;

	virtual FIndex VoxelIndexToChunkIndex(FIndex InIndex) const;

	virtual uint64 VoxelIndexToNumber(FIndex InIndex, bool bWorldSpace = false) const;

	virtual FIndex NumberToVoxelIndex(uint64 InNumber, bool bWorldSpace = false) const;

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
	
	UPROPERTY(VisibleAnywhere, Category = "Chunk")
	int32 ChunkSpawnBatch;

	UPROPERTY(VisibleAnywhere, Category = "Chunk")
	FIndex ChunkGenerateIndex;

	UPROPERTY(Transient)
	TMap<FIndex, AVoxelChunk*> ChunkMap;

public:
	virtual int32 GetChunkNum(bool bNeedGenerated = false) const;

	virtual bool IsChunkGenerated(FIndex InIndex) const;
	
	virtual FVoxelChunkQueues GetChunkQueues(EVoxelWorldState InWorldState) const;

protected:
	UPROPERTY(EditAnywhere, Category = "Voxel")
	TArray<TSubclassOf<UVoxel>> VoxelClasses;

	UPROPERTY(EditAnywhere, Instanced, Category = "Voxel")
	TArray<UVoxelGenerator*> VoxelGenerators;

	UPROPERTY(Transient)
	TMap<TSubclassOf<UVoxelGenerator>, UVoxelGenerator*> VoxelGeneratorMap;

public:
	template<class T>
	T* GetVoxelGenerator() const
	{
		return Cast<T>(GetVoxelGenerator(T::StaticClass()));
	}
	virtual UVoxelGenerator* GetVoxelGenerator(const TSubclassOf<UVoxelGenerator>& InClass) const;
};
