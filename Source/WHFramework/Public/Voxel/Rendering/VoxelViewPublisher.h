#pragma once

#include "CoreMinimal.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Rendering/VoxelPublishGroups.h"

class AActor;
class UVoxelModule;
class UVoxelMeshComponent;

struct WHFRAMEWORK_API FVoxelViewCoverageResult final : FVoxelTaskCustomPayload
{
	TArray<TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>> Meshes;
	uint64 OwnedMeshBytes = 0;
	virtual uint64 GetAllocatedBytes() const override;
};

class WHFRAMEWORK_API FVoxelViewPublisher
{
public:
	FVoxelViewPublisher(UVoxelModule& InModule, FVoxelTaskScheduler& InScheduler, uint64 InWorldEpoch);
	~FVoxelViewPublisher();
	bool Stage(
		AActor*& InOutActor,
		const FVector& InLocation,
		double InScale,
		FVoxelSectionMeshResult&& InMesh,
		int32 InTerrainStage,
		const FVoxelPublishGroupKey& InGroupKey,
		bool bInTransition = false);
	void BeginBatch();
	void SetCoverage(AActor* InActor, TArray<FBox> InWorldCellBoxes,
		TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes = nullptr);
	void SetHidden(AActor* InActor, bool bInHidden);
	bool EndBatch(TFunction<void()> InOnCommitted = {},
		TFunction<void(TConstArrayView<FVoxelPublishGroupKey>)> InOnGroupCommitted = {});
	void Tick();
	void Forget(AActor* InActor);
	void Reset();
	bool IsBusy() const;
	bool IsGroupBusy(const FVoxelPublishGroupKey& InKey) const;
	bool NeedsUpdate() const;
	bool IsPresented(AActor* InActor) const;
	bool IsCommitted(AActor* InActor) const;
	bool HasPresentation(AActor* InActor) const;

private:
	using FMeshPtr = TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>;

	struct FEntry
	{
		FMeshPtr Source;
		int32 TerrainStage = INDEX_NONE;
		FVector Location = FVector::ZeroVector;
		double Scale = 1.0;
		FBox Bounds = FBox(ForceInit);
		FBox PresentedWorldBounds = FBox(ForceInit);
		FVoxelPublishGroupKey GroupKey;
		TArray<FBox> Exclusions;
		TArray<TObjectPtr<UVoxelMeshComponent>> Components;
		bool bTransition = false;
		bool bDirty = true;
		bool bPresented = false;
	};

	struct FUpdate
	{
		TWeakObjectPtr<AActor> Actor;
		FMeshPtr Source;
		int32 TerrainStage = INDEX_NONE;
		TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> CommonBoxes;
		FBox Bounds = FBox(ForceInit);
		TArray<FBox> PreviousExclusions;
		bool bSourceDirty = true;
		bool bUnchanged = false;
		int32 GroupIndex = INDEX_NONE;
		int32 PreparedBatchIndex = 0;
		FVector Location = FVector::ZeroVector;
		double Scale = 1.0;
		TArray<FBox> Exclusions;
		TArray<TObjectPtr<UVoxelMeshComponent>> Components;
	};

	struct FGroup
	{
		TArray<int32> UpdateIndices;
		TArray<TWeakObjectPtr<AActor>> VisibilityActors;
		TArray<FVoxelPublishGroupKey> Keys;
		int32 PreparedUpdates = 0;
		bool bCommitted = false;
	};

	void BuildGroups();
	void AdmitCoveragePreparation();
	void AdmitBuilds();
	bool PrepareUpdate(int32 InIndex, double InDeadline, int32& InOutPreparedComponents, bool& bOutComplete);
	void CommitReadyGroups();
	void CommitGroup(FGroup& InGroup);
	void FinishBatch();
	void DiscardBatch();

private:
	UVoxelModule& Module;
	FVoxelTaskScheduler& Scheduler;
	uint64 WorldEpoch = 0;
	uint64 BatchSerial = 0;
	TMap<TWeakObjectPtr<AActor>, FEntry> Entries;
	TArray<FUpdate> Updates;
	TArray<FGroup> Groups;
	TMap<TWeakObjectPtr<AActor>, bool> Visibility;
	TArray<FMeshPtr> PreparedMeshes;
	TArray<TWeakObjectPtr<UVoxelMeshComponent>> RetiredComponents;
	TFunction<void()> OnCommitted;
	TFunction<void(TConstArrayView<FVoxelPublishGroupKey>)> OnGroupCommitted;
	int32 CoverageIndex = 0;
	int32 BuildIndex = 0;
	int32 PendingBuilds = 0;
	bool bBusy = false;
	bool bCoveragePrepared = false;
	bool bCoveragePreparing = false;
	bool bRetry = false;
};
