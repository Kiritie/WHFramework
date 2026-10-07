#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "UObject/StrongObjectPtr.h"
#include "Voxel/Generation/VoxelStructure.h"
#include "Voxel/Rendering/VoxelDetailData.h"
#include "Voxel/Generation/VoxelGenerationConsumer.h"

class AActor;
class FVoxelTaskScheduler;
class UVoxelModule;
struct FVoxelTaskResult;
struct FVoxelDetailAssetBatch;
class FVoxelTerrainResourceLease;
class FVoxelTerrainWorkReceipt;

struct WHFRAMEWORK_API FVoxelDetailPlan
{
	uint64 Token = 0;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> WorkReceipt;
	TArray<FVoxelStructureDetailPlacement> Placements;

	uint64 GetAllocatedBytes() const;
};

/**
 * Near vegetation/detail presentation. It deliberately owns presentation only;
 * placement truth remains in the frozen structure/feature plans.
 */
class WHFRAMEWORK_API FVoxelDetailView
{
public:
	FVoxelDetailView(
		UVoxelModule& InModule,
		FVoxelTaskScheduler& InScheduler,
		uint64 InWorldEpoch);
	~FVoxelDetailView();

	bool Initialize(FString& OutError);
	void Tick(TConstArrayView<FVector> InObservers);
	bool OnTask(FVoxelTaskResult&& InResult);
	void Reset();

private:
	bool BuildAssetBatch(int32 InAssetIndex, bool& bOutComplete, FString& OutError);
	void DestroyActors(TArray<TWeakObjectPtr<AActor>>& InActors);
	void AbortStaging();
	void MarkDirty(const struct FVoxelEditBatch& InBatch);
	void CancelPlanning();


private:
	UVoxelModule& Module;
	FVoxelTaskScheduler& Scheduler;
	uint64 WorldEpoch = 0;
	FGuid OwnerId = FGuid::NewGuid();
	uint64 OwnerIncarnation = 1;
	uint64 NextToken = 1;
	uint64 WaitingToken = 0;
	uint64 CapacityVersion = 0;
	bool bCapacityBlocked = false;
	double LastRequestTime = -100.0;
	TArray<FIntVector> LastObserverSections;
	TArray<FVector> Observers;
	TArray<FIntVector> PlannedObserverCells;
	TArray<FVoxelGenerationConsumerKey> FactConsumers;
	TArray<TStrongObjectPtr<UVoxelDetailData>> Assets;
	TArray<TStrongObjectPtr<UStaticMesh>> Meshes;
	TMap<FName, int32> AssetIndices;
	TSharedPtr<const FVoxelDetailPlan, ESPMode::ThreadSafe> PendingPlan;
	TSharedPtr<FVoxelDetailAssetBatch, ESPMode::ThreadSafe> AssetBatch;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> StorageLease;
	TArray<TWeakObjectPtr<AActor>> RetiringActors;
	TArray<TWeakObjectPtr<AActor>> ActiveActors;
	TArray<TWeakObjectPtr<AActor>> StagedActors;
	FDelegateHandle CommitHandle;
	int32 StagedAssetIndex = 0;
	int32 PresentationCursor = 0;
	int32 ObserverCapacity = 8;
	bool bRunning = false;
	bool bDirty = true;
	bool bStopped = false;
};
