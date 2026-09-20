#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "UObject/StrongObjectPtr.h"
#include "Voxel/Generation/VoxelStructure.h"
#include "Voxel/Rendering/VoxelDetailData.h"

class AActor;
class FVoxelTaskScheduler;
class UVoxelModule;
struct FVoxelTaskResult;

struct WHFRAMEWORK_API FVoxelDetailPlan
{
	uint64 Token = 0;
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
	bool BuildAssetBatch(int32 InAssetIndex, FString& OutError);
	void DestroyActors(TArray<TWeakObjectPtr<AActor>>& InActors);
	void AbortStaging();
	void MarkDirty(const struct FVoxelEditBatch& InBatch);


private:
	UVoxelModule& Module;
	FVoxelTaskScheduler& Scheduler;
	uint64 WorldEpoch = 0;
	uint64 NextToken = 1;
	uint64 WaitingToken = 0;
	double LastRequestTime = -100.0;
	TArray<FIntVector> LastObserverSections;
	TArray<FVector> Observers;
	TArray<TStrongObjectPtr<UVoxelDetailData>> Assets;
	TArray<TStrongObjectPtr<UStaticMesh>> Meshes;
	TMap<FName, int32> AssetIndices;
	TSharedPtr<const FVoxelDetailPlan, ESPMode::ThreadSafe> PendingPlan;
	TArray<TWeakObjectPtr<AActor>> ActiveActors;
	TArray<TWeakObjectPtr<AActor>> StagedActors;
	FDelegateHandle CommitHandle;
	int32 StagedAssetIndex = 0;
	bool bRunning = false;
	bool bDirty = true;
	bool bStopped = false;
};
