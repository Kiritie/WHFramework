#pragma once

#include "Containers/Queue.h"
#include "VoxelModuleTypes.h"
#include "Chunks/VoxelChunk.h"
#include "Common/CommonModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "VoxelModule.generated.h"

class AVoxelRoot;
class UVoxelGenerator;
class AVoxelCapture;
class UVoxelChunk;
class UVoxelData;
class ACharacterBase;
class UWorldTimer;
class UWorldWeather;
class AVoxelEntityCapture;
class UDataTable;

/**
 * 体素模块
 */
UCLASS()
class WHFRAMEWORK_API UVoxelModule : public UModuleBase
{
	GENERATED_BODY()

	friend class UVoxelChunk;
	
	GENERATED_MODULE(UVoxelModule)

public:	
	UVoxelModule();

	~UVoxelModule();

	//////////////////////////////////////////////////////////////////////////
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
protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Root")
	AVoxelRoot* VoxelRoot;

public:
	UFUNCTION(BlueprintPure)
	AVoxelRoot* GetVoxelRoot() const { return VoxelRoot; }
	
	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Capture")
	AVoxelCapture* VoxelCapture;

	UPROPERTY(Transient)
	TArray<AVoxelEntityCapture*> CaptureVoxels;

public:
	UFUNCTION(BlueprintPure)
	AVoxelCapture* GetVoxelCapture() const { return VoxelCapture; }

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditAnywhere, Category = "World")
	bool bAutoGenerate;

	UPROPERTY(EditAnywhere, Category = "World")
	EVoxelWorldMode WorldMode;

	UPROPERTY(VisibleAnywhere, Category = "World")
	EVoxelWorldState WorldState;

	UPROPERTY(VisibleAnywhere, Category = "World")
	EVoxelGenerationStage WorldGenerationStage;

	UPROPERTY(EditAnywhere, Category = "World")
	FVoxelWorldBasicSaveData WorldBasicData;
	
	UPROPERTY(VisibleAnywhere, Category = "World")
	FIndex WorldCenterIndex;
	
	UPROPERTY(VisibleAnywhere, Category = "World")
	FIndex WorldAgentIndex;

public:
	UFUNCTION(BlueprintPure)
	EVoxelWorldMode GetWorldMode() const { return WorldMode; }

	UFUNCTION(BlueprintCallable)
	void SetWorldMode(EVoxelWorldMode InWorldMode);

	UFUNCTION(BlueprintPure)
	EVoxelWorldState GetWorldState() const { return WorldState; }

	UFUNCTION(BlueprintPure)
	EVoxelGenerationStage GetWorldGenerationStage() const { return WorldGenerationStage; }

	UFUNCTION(BlueprintPure)
	FVoxelWorldBasicSaveData& GetWorldBasicData() { return WorldBasicData; }

	UFUNCTION(BlueprintPure)
	FIndex GetWorldCenterIndex() const { return WorldCenterIndex; }

	UFUNCTION(BlueprintPure)
	FIndex GetWorldAgentIndex() const { return WorldAgentIndex; }

protected:
	UFUNCTION(BlueprintCallable)
	void SetWorldState(EVoxelWorldState InWorldState);

protected:
	virtual void OnWorldModeChanged();

	virtual void OnWorldStateChanged();

	virtual void OnWorldCenterChanged();

	virtual void OnWorldAgentMoved();

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

	void InitializeSceneAreaNames();

	FSceneArea ResolveChunkSceneArea(const FSceneArea& InArea, const FVector2D& InPoint) const;

	TMap<EVoxelAreaType, TArray<FText>> SceneAreaNamePrefixes;

public:
	virtual void LoadPrefabData(const FVoxelPrefabSaveData& InPrefabData);

	virtual FVoxelPrefabSaveData GetPrefabData();

protected:
	virtual void GenerateWorld();

public:
	void AddVoxelUpdate(FIndex InIndex);

	void AddVoxelLiquidUpdate(FIndex InIndex);

protected:
	void UpdateVoxels();

	void ApplyVoxelUpdates(const TMap<FIndex, FVoxelItem>& InVoxelMap, TSet<FIndex>& OutChangedChunkIndices);
	
public:
	virtual UVoxelChunk* SpawnChunk(FIndex InIndex, bool bAddToQueue = true);

	virtual void LoadChunkMap(FIndex InIndex);

	virtual void BuildChunkMap(FIndex InIndex, int32 InStage);

	virtual void SpawnChunkMesh(FIndex InIndex, int32 InStage);

	virtual void BuildChunkMesh(FIndex InIndex);

	virtual void GenerateChunk(FIndex InIndex);

	virtual void SaveChunk(FIndex InIndex);

	virtual void UnloadChunk(FIndex InIndex);
	
public:
	virtual void GenerateChunkQueues(bool bFromAgent = true, bool bForce = false);

	virtual void ResetChunkQueues();

protected:
	virtual bool UpdateChunkQueue(EVoxelWorldState InState, TFunction<void(FIndex)> InFunc);
	
	virtual bool UpdateChunkQueue(EVoxelWorldState InState, TFunction<void(FIndex, int32)> InFunc);

	virtual void UpdateChunkQueueThreads();

	virtual bool DispatchChunkQueue(FVoxelChunkQueue& InQueue, const TFunction<void(FIndex, int32)>& InFunc, int32 InStage);

	virtual void CancelChunkQueueBatch();

	virtual void ShutdownChunkQueueThreads();

	virtual void AddToChunkQueue(EVoxelWorldState InState, FIndex InIndex);
	
	virtual void RemoveFromChunkQueue(EVoxelWorldState InState, FIndex InIndex);

	FVoxelChunkQueues& GetMutableChunkQueues(EVoxelWorldState InWorldState);

public:
	virtual void GenerateVoxelStage(UVoxelChunk* InChunk, int32 InStage) const;

public:
	virtual bool IsOnTheWorld(FIndex InIndex, bool bIgnoreZ = true) const;

	virtual UVoxelChunk* GetChunkByIndex(FIndex InIndex) const;

	virtual UVoxelChunk* GetChunkByLocation(FVector InLocation) const;

	virtual UVoxelChunk* GetChunkByVoxelIndex(FIndex InIndex) const;
		
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

	FVoxelTopography SampleBaseTopographyByIndex(FIndex InIndex) const;

	FVoxelTopography SampleTopographyByIndex(FIndex InIndex) const;

	UFUNCTION(BlueprintPure)
	EVoxelRegionType GetWorldRegionByIndex(FIndex InIndex) const;

	UFUNCTION(BlueprintPure)
	FText GetWorldAreaPrefix(FIndex InIndex, EVoxelAreaType InNameType) const;

	UFUNCTION(BlueprintPure)
	FText GetWorldRegionDisplayName(EVoxelRegionType InRegionType) const;

	FText GetWorldAreaDisplayName(FIndex InIndex, EVoxelAreaType InNameType, const FText& InAreaTypeName) const;

	UFUNCTION(BlueprintPure)
	FText GetWorldAreaDisplayNameByIndex(FIndex InIndex) const;

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
	TSubclassOf<UVoxelChunk> ChunkSpawnClass;

	UPROPERTY(EditAnywhere, Category = "Chunk")
	float ChunkSpawnDistance;

	UPROPERTY(EditAnywhere, Category = "Chunk|Pipeline")
	FVoxelChunkQueues SpawningQueues;

	UPROPERTY(EditAnywhere, Category = "Chunk|Pipeline")
	FVoxelChunkQueues MapLoadingQueues;

	UPROPERTY(EditAnywhere, Category = "Chunk|Pipeline")
	FVoxelChunkQueues MapBuildingQueues;

	UPROPERTY(EditAnywhere, Category = "Chunk|Pipeline")
	FVoxelChunkQueues MeshSpawningQueues;

	UPROPERTY(EditAnywhere, Category = "Chunk|Pipeline")
	FVoxelChunkQueues MeshBuildingQueues;

	UPROPERTY(EditAnywhere, Category = "Chunk|Pipeline")
	FVoxelChunkQueues GeneratingQueues;

	UPROPERTY(EditAnywhere, Category = "Chunk|Pipeline")
	FVoxelChunkQueues UnloadingQueues;

	TArray<FVoxelChunkQueueThread*> ChunkQueueThreads;

	TSharedPtr<FVoxelChunkQueueBatch, ESPMode::ThreadSafe> ActiveChunkQueueBatch;

	FVoxelChunkQueue* ActiveChunkQueue;

	TArray<FVoxelChunkQueueThread*> ActiveChunkQueueThreads;

	UPROPERTY(VisibleAnywhere, Category = "Chunk")
	int32 ChunkSpawnBatch;

	UPROPERTY(Transient)
	TMap<FIndex, UVoxelChunk*> ChunkMap;

	TSet<FIndex> VoxelUpdateChunkIndices;

	TQueue<FIndex, EQueueMode::Mpsc> VoxelUpdateQueue;

	TSet<FIndex> VoxelLiquidUpdateIndices;

	TQueue<FIndex, EQueueMode::Mpsc> VoxelLiquidUpdateQueue;

	float VoxelUpdateTime;

	bool bVoxelUpdateRunning;

public:
	virtual int32 GetChunkNum(bool bNeedGenerated = false) const;

	virtual bool IsChunkGenerated(FIndex InIndex) const;
	
	virtual FVoxelChunkQueues GetChunkQueues(EVoxelWorldState InWorldState) const;

protected:
	UPROPERTY(EditAnywhere, Category = "World|Area")
	UDataTable* SceneAreaNameTable;

	UPROPERTY(EditAnywhere, Category = "Voxel")
	TArray<TSubclassOf<UVoxel>> VoxelClasses;

	UPROPERTY(Transient)
	TMap<TSubclassOf<UVoxelGenerator>, UVoxelGenerator*> VoxelGeneratorMap;

	UPROPERTY(Transient)
	TMap<EVoxelType, FPrimaryAssetId> VoxelAssetIDMap;

public:
	template<class T>
	T* GetVoxelGenerator() const
	{
		return Cast<T>(GetVoxelGenerator(T::StaticClass()));
	}
	virtual UVoxelGenerator* GetVoxelGenerator(const TSubclassOf<UVoxelGenerator>& InClass) const;

	UFUNCTION(BlueprintPure)
	FPrimaryAssetId VoxelTypeToAssetID(EVoxelType InVoxelType) const;
};
