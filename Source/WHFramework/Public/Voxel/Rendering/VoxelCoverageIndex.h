#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Rendering/VoxelGameThreadContinuation.h"

struct WHFRAMEWORK_API FVoxelCoverageRecord
{
	FVoxelTerrainGeometry Geometry;
	FVoxelTerrainBuildStamp Stamp;
	FIntVector Min = FIntVector::ZeroValue;
	FIntVector Max = FIntVector::ZeroValue;
	bool bWanted = false;
	bool bReady = false;
	bool bCommitted = false;
	bool bHasGeometry = false;
};

struct WHFRAMEWORK_API FVoxelCoverageIndexKey
{
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	uint8 Level = 0;
	bool operator==(const FVoxelCoverageIndexKey& InOther) const = default;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelCoverageIndexKey& InKey)
{
	return HashCombineFast(GetTypeHash(InKey.Coordinate), GetTypeHash(InKey.Level));
}

struct WHFRAMEWORK_API FVoxelCoverageReadGuard
{
	FVoxelCoverageIndexKey Key;
	uint64 Version = 0;
	bool bSubtree = false;
};

struct WHFRAMEWORK_API FVoxelCoverageRecordReadGuard
{
	FVoxelTerrainSpatialKey Key;
	uint64 Version = 0;
};

struct WHFRAMEWORK_API FVoxelCoverageLocalSnapshot
{
	uint64 StorageGeneration = 0;
	uint64 MembershipVersion = 0;
	bool bFullIndex = false;
	bool bIncludePresentation = true;
	TArray<TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe>> Records;
	TArray<FVoxelCoverageReadGuard> Guards;
	TArray<FVoxelCoverageRecordReadGuard> RecordGuards;
	int32 VisitedIndexNodes = 0;
	int32 ExaminedRecords = 0;
};

// 由 ViewManager 单一持有；工作线程只持有不可变 Records 与版本守卫。
class WHFRAMEWORK_API FVoxelCoverageIndex
{
public:
	bool Upsert(TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe> InRecord, FString& OutError);
	bool Remove(const FVoxelTerrainSpatialKey& InKey);
	TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe> Find(const FVoxelTerrainSpatialKey& InKey) const { return Records.FindRef(InKey); }
	void Capture(const FIntVector& InMin, const FIntVector& InMax, FVoxelCoverageLocalSnapshot& OutSnapshot,
		bool bInIncludePresentation = true) const;
	FVoxelGameThreadContinuation CaptureSteps(
		FIntVector InMin, FIntVector InMax, FVoxelCoverageLocalSnapshot& OutSnapshot, bool bInIncludePresentation = true) const;
	FVoxelGameThreadContinuation CaptureAllSteps(FVoxelCoverageLocalSnapshot& OutSnapshot, bool bInIncludePresentation = true) const;
	bool IsCurrent(const FVoxelCoverageLocalSnapshot& InSnapshot) const;
	void Reset();
	void PruneEmptyNodes(int32 InOutstandingSnapshots);
	int32 NumRecords() const { return Records.Num(); }
	int32 NumIndexNodes() const { return Nodes.Num(); }

private:
	struct FNode
	{
		uint64 OwnVersion = 0;
		uint64 SubtreeVersion = 0;
		int32 SubtreeRecords = 0;
		TSet<FVoxelTerrainSpatialKey> Records;
	};
	struct FRecordVersions
	{
		uint64 Inputs = 0;
		uint64 Presentation = 0;
	};
	struct FQuery
	{
		int64 MinX = 0;
		int64 MinY = 0;
		int64 MaxX = 0;
		int64 MaxY = 0;
		FIntVector ExactMin = FIntVector::ZeroValue;
		FIntVector ExactMax = FIntVector::ZeroValue;
	};
	static FVoxelCoverageIndexKey Parent(const FVoxelCoverageIndexKey& InKey);
	void Touch(const FVoxelCoverageIndexKey& InKey, int32 InCountDelta);
	void CaptureNode(const FVoxelCoverageIndexKey& InKey, const FQuery& InQuery,
		FVoxelCoverageLocalSnapshot& OutSnapshot) const;
	void CollectSubtree(const FVoxelCoverageIndexKey& InKey, const FQuery& InQuery,
		FVoxelCoverageLocalSnapshot& OutSnapshot) const;
	void AppendOwn(const FNode& InNode, const FQuery& InQuery, FVoxelCoverageLocalSnapshot& OutSnapshot) const;
	FVoxelGameThreadContinuation CaptureNodeSteps(
		FVoxelCoverageIndexKey InKey, FQuery InQuery, FVoxelCoverageLocalSnapshot& OutSnapshot, bool bInCollect = false) const;

	TMap<FVoxelTerrainSpatialKey, TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe>> Records;
	TMap<FVoxelTerrainSpatialKey, FRecordVersions> RecordVersions;
	TMap<FVoxelCoverageIndexKey, FNode> Nodes;
	TSet<FVoxelCoverageIndexKey> RootKeys;
	uint64 Serial = 0;
	uint64 MembershipVersion = 0;
	uint64 StorageGeneration = 1;
};
