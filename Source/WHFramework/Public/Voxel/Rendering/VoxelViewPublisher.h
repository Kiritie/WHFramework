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
	FGuid BeginGroup(const FVoxelPublishGroupKey& InKey);
	bool Stage(
		const FGuid& InGroup,
		AActor*& InOutActor,
		const FVector& InLocation,
		double InScale,
		FVoxelSectionMeshResult&& InMesh,
		int32 InTerrainStage,
		bool bInTransition = false);
	bool SetCoverage(const FGuid& InGroup, AActor* InActor, TArray<FBox> InWorldCellBoxes,
		TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes = nullptr);
	bool SetSharedCoverage(const FGuid& InGroup, AActor* InActor,
		TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes);
	bool SetHidden(const FGuid& InGroup, AActor* InActor, bool bInHidden);
	void SetCommitCallbacks(TFunction<void()> InOnCommitted,
		TFunction<void(TConstArrayView<FVoxelPublishGroupKey>)> InOnGroupCommitted);
	bool CommitGroup(const FGuid& InGroup);
	bool SetOwnershipChanged(const FGuid& InGroup, bool bInChanged);
	bool SetTerrainStage(const FGuid& InGroup, int32 InTerrainStage);
	void SetObservers(TConstArrayView<FVector> InObservers);
	void Tick();
	void Forget(AActor* InActor);
	void Reset();
	bool IsBusy() const;
	bool HasActiveGroups() const { return !Publications.IsEmpty(); }
	bool IsGroupBusy(const FVoxelPublishGroupKey& InKey) const;
	bool NeedsUpdate() const;
	bool IsPresented(AActor* InActor) const;
	bool IsCommitted(AActor* InActor) const;
	bool HasPresentation(AActor* InActor) const;
	FVoxelFrameTimings GetFrameTimings() const { return FrameTimings; }

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FVoxelConcurrentPublishCommitTest;
	friend class FVoxelPublisherStageAndIndexTest;
#endif
	using FMeshPtr = TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>;
	using FGroupCallback = TFunction<void(TConstArrayView<FVoxelPublishGroupKey>)>;
	using FGroupCallbackPtr = TSharedPtr<FGroupCallback>;
	struct FGroupPriority
	{
		// GT 距离按世界 XY 平方；worker 距离按 streaming source 的 cell 空间。
		double ObserverDistance = MAX_dbl;
		double DistanceScore = MAX_dbl;
		int32 SourcePriority = MAX_int32;
		int32 TerrainStage = 3;
		EVoxelWorkClass WorkClass = EVoxelWorkClass::Visible;
	};
	struct FDeferredPriorityDemand
	{
		TArray<FBox> Bounds;
		uint64 Serial = 0;
		int32 TerrainStage = 3;
		bool bTransition = false;
	};
	using FPriorityBoundsPtr = TSharedPtr<const FDeferredPriorityDemand, ESPMode::ThreadSafe>;

	struct FEntry
	{
		FMeshPtr Source;
		FMeshPtr PresentedSource;
		int32 TerrainStage = INDEX_NONE;
		FVector Location = FVector::ZeroVector;
		double Scale = 1.0;
		FBox Bounds = FBox(ForceInit);
		FBox PresentedWorldBounds = FBox(ForceInit);
		FVoxelPublishGroupKey GroupKey;
		TArray<FBox> Exclusions;
		TArray<FBox> LocalWorldBoxes;
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
		TArray<FBox> ChangedBounds;
		TArray<FBox> LocalWorldBoxes;
		bool bSourceDirty = true;
		bool bRetainSource = false;
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
		FBox Bounds = FBox(ForceInit);
		TArray<FBox> ChangedBounds;
		// Proxy 接缝可能服务于 Fine 交接，实际更新阶段不能仅由 owner 类别推断。
		int32 TerrainStage = 3;
		bool bTransition = false;
		TArray<int32> UpdateIndices;
		TArray<TWeakObjectPtr<AActor>> VisibilityActors;
		TArray<FVoxelPublishGroupKey> Keys;
		FGroupPriority Priority;
		int32 PreparedUpdates = 0;
		bool bCommitted = false;
	};

	struct FPendingGroup
	{
		FVoxelPublishGroupKey Key;
		bool bCommitted = false;
		FGroupCallbackPtr Callback;
		bool bOwnershipChanged = true;
		FPriorityBoundsPtr DeferredPriorityBounds;
		int32 TerrainStage = 3;
	};

	struct FPublication
	{
		uint64 Serial = 0;
		TArray<FUpdate> Updates;
		TArray<FGroup> Groups;
		TArray<int32> GroupOrder;
		TMap<TWeakObjectPtr<AActor>, bool> Visibility;
		TMap<FVoxelPublishGroupKey, FGroupCallbackPtr> Callbacks;
		TMap<FVoxelPublishGroupKey, int32> TerrainStages;
		TSet<FVoxelPublishGroupKey> OwnershipChanges;
		TMap<FVoxelPublishGroupKey, FPriorityBoundsPtr> DeferredPriorityBounds;
		TArray<FMeshPtr> PreparedMeshes;
		int32 CoverageIndex = 0;
		TArray<int32> BuildOrder;
		TSet<int32> PendingBuildIndices;
		bool bBuildOrderInitialized = false;
		bool bCoveragePrepared = false;
		bool bCoveragePreparing = false;
		bool bGroupsPrepared = true;
		bool bFinished = false;
		bool bCanceled = false;
	};
	using FPublicationRef = TSharedRef<FPublication>;

	void StartCommittedGroups();
	void BuildGroups(const FPublicationRef& Publication, bool bInPreparedCoverage = false);
	void DeferSaturatedGroups(const FPublicationRef& Publication, TMap<FVoxelPublishGroupKey, FGroupPriority>& OutPriorities);
	void UpdateGroupPriorities(TMap<FVoxelPublishGroupKey, FGroupPriority> InPriorities);
	FUpdate TakePendingUpdate(int32 InIndex);
	void SortGroups(const FPublicationRef& Publication);
	void RefreshGroupOrder(const FPublicationRef& Publication);
	double GroupDistance(const FGroup& InGroup) const;
	double BoundsDistance(TConstArrayView<FBox> InBounds) const;
	static int32 GroupStage(const FGroup& InGroup);
	bool CanPublishGroup(const FGroup& InGroup) const;
	static void MergePriority(FGroupPriority& InOutPriority, const FGroupPriority& InPriority);
	static bool HigherGroupPriority(const FGroupPriority& InFirst, const FGroupPriority& InSecond);
	FGroupPriority BoundsPriority(TConstArrayView<FBox> InBounds, int32 InStage) const;
	FGroupPriority GroupPriority(const FGroup& InGroup) const;
	static bool GroupsInteract(const FGroup& InFirst, const FGroup& InSecond);
	void SetPublicationPriority(FVoxelTaskRequest& InOutRequest, const FPublication& InPublication, TConstArrayView<int32> InGroupIndices) const;
	void RefreshTaskPriorities();
	void AdmitCoveragePreparation(const FPublicationRef& Publication);
	void AdmitBuilds(const FPublicationRef& Publication);
	bool PrepareUpdate(const FPublicationRef& Publication, int32 InIndex, double InDeadline, int32& InOutPreparedComponents, bool& bOutComplete);
	void CommitReadyGroups(const FPublicationRef& Publication, int32& InOutCommitted, double InDeadline,
		const TMap<FVoxelPublishGroupKey, uint64>& InFirstOwners);
	void CommitReadyGroup(const FPublicationRef& Publication, FGroup& InGroup);
	void FinishGroups();
	void DiscardGroups();
	void DiscardPublication(const FPublicationRef& Publication);

private:
	UVoxelModule& Module;
	FVoxelTaskScheduler& Scheduler;
	uint64 WorldEpoch = 0;
	uint64 BatchSerial = 0;
	TMap<TWeakObjectPtr<AActor>, FEntry> Entries;
	TMap<FGuid, FPendingGroup> PendingGroups;
	TMap<FVoxelPublishGroupKey, FGuid> PendingGroupIds;
	TArray<FUpdate> PendingUpdates;
	TMap<TWeakObjectPtr<AActor>, int32> PendingUpdateIndices;
	TMap<TWeakObjectPtr<AActor>, bool> PendingVisibility;
	TArray<FPublicationRef> Publications;
	// 发布分组的派生查询索引，避免每个 Actor 重扫全部在途 owner。
	mutable TSet<FVoxelPublishGroupKey> ActiveBusyKeys;
	mutable bool bActiveBusyKeysDirty = true;
	TArray<FVector> Observers;
	TArray<TWeakObjectPtr<UVoxelMeshComponent>> RetiredComponents;
	TFunction<void()> OnCommitted;
	FGroupCallbackPtr CurrentGroupCallback;
	bool bRetry = false;
	FVoxelFrameTimings FrameTimings;
};
