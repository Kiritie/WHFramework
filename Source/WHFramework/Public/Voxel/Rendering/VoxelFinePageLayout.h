#pragma once

#include "CoreMinimal.h"
#include "Voxel/Streaming/VoxelFineSpatial.h"

struct WHFRAMEWORK_API FVoxelFineLevelAnchors
{
	FGuid Source;
	TArray<FIntVector> Levels;
};

struct WHFRAMEWORK_API FVoxelFineLayoutPage
{
	FVoxelFinePageKey Key;
	TArray<FVoxelFineShardKey> Shards;
};

struct WHFRAMEWORK_API FVoxelFinePageLayoutDelta
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	bool bChanged = false;
	TArray<FVoxelFineLayoutPage> Added;
	TSet<FVoxelFinePageKey> Removed;
	TSet<FVoxelFinePageKey> Retained;
};

// Page 只组合不可变 Shard 几何；调整 Level 不生成或持有 Runtime Section。
class WHFRAMEWORK_API FVoxelFinePageLayout
{
public:
	TSharedPtr<FVoxelFinePageLayout, ESPMode::ThreadSafe> Capture(
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, FString& OutError) const;
	uint64 GetAllocatedBytes() const;
	bool Initialize(uint8 InMaximumLevel, int32 InShardSideCells, int32 InFirstMergeDistanceCells, FString& OutError);
	bool SetSource(const FGuid& InSource, const FIntVector& InPosition, FString& OutError);
	bool RemoveSource(const FGuid& InSource, FString& OutError);
	bool ApplyResidencyDelta(const TVoxelFineSetDelta<FVoxelFineShardKey>& InDelta, FString& OutError);
	int32 Advance(int32 InMaximumRoots, FVoxelFinePageLayoutDelta& OutDelta,
		FString* OutError = nullptr, bool bInProduceDelta = true);
	const TMap<FVoxelFinePageKey, FVoxelFineLayoutPage>& GetPages() const;
	const TMap<FGuid, FVoxelFineLevelAnchors>& GetAnchors() const;
	bool IsPending() const;
	void Reset();

private:
	bool GrantStorage(uint64 InBytes, FString& OutError);
	void SettleStorage();
	bool ReserveSource(int32 InSourceCount, FString& OutError);
	bool ReserveResidencyDelta(const TVoxelFineSetDelta<FVoxelFineShardKey>& InDelta, FString& OutError);
	bool ReserveAdvance(const FVoxelFinePageKey& InRoot, int32 InShardCount, FVoxelFinePageLayoutDelta& OutDelta,
		bool bInProduceDelta, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutScratch, FString& OutError);
	FVoxelFinePageKey RootFor(const FVoxelFineShardKey& InShard) const;
	void DirtyRoot(const FVoxelFinePageKey& InRoot);
	void DirtyInfluence(const FVoxelFineLevelAnchors& InAnchors);
	bool CanMerge(const FVoxelFinePageKey& InParent) const;
	void BuildRoot(const FVoxelFinePageKey& InRoot, TMap<FVoxelFinePageKey, FVoxelFineLayoutPage>& OutPages) const;

private:
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> StorageLease;
	uint8 MaximumLevel = 3;
	uint8 MinimumLevel = 1;
	int32 ShardSideCells = 32;
	int32 FirstMergeDistanceCells = 64;
	TMap<FGuid, FVoxelFineLevelAnchors> Anchors;
	TMap<FVoxelFinePageKey, TSet<FVoxelFineShardKey>> RootShards;
	TMap<FVoxelFinePageKey, FVoxelFineLayoutPage> Pages;
	TMap<FVoxelFinePageKey, TSet<FVoxelFinePageKey>> RootPages;
	TArray<FVoxelFinePageKey> DirtyQueue;
	TSet<FVoxelFinePageKey> QueuedRoots;
	int32 DirtyCursor = 0;
};
