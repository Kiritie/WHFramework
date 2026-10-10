#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Rendering/VoxelGameThreadContinuation.h"
#include "Voxel/Generation/VoxelGenerationConsumer.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"

class FVoxelTerrainResources;
class FVoxelTerrainResourceLease;

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
	FIntVector Coordinate = FIntVector::ZeroValue;
	uint8 Level = 0;
	// Heightfield 表示世界高度列；Fine／Proxy 必须按独立 Z 层索引。
	bool bColumn = false;
	bool operator==(const FVoxelCoverageIndexKey& InOther) const = default;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelCoverageIndexKey& InKey)
{
	return HashCombineFast(HashCombineFast(GetTypeHash(InKey.Coordinate), GetTypeHash(InKey.Level)), GetTypeHash(InKey.bColumn));
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

// 已捕获数组只共享不可变 backing；复制读守卫不在 GT 再复制整份局部数组。
template<typename ElementType>
class TVoxelCoverageSnapshotArray
{
	friend class FVoxelCoverageIndex;
	struct FStorage { TArray<ElementType> Values; };
	TSharedPtr<FStorage, ESPMode::ThreadSafe> Storage;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	static const TArray<ElementType>& EmptyArray()
	{
		static const TArray<ElementType> Empty;
		return Empty;
	}
	const TArray<ElementType>& Values() const { return Storage ? Storage->Values : EmptyArray(); }
	TArray<ElementType>& MutableValues()
	{
		if (!Storage)
			Storage = Lease ? VoxelBoundedAllocation::MakeWorkerShared<FStorage>(Lease) : MakeShared<FStorage, ESPMode::ThreadSafe>();
		check(Storage.IsUnique());
		return Storage->Values;
	}
public:
	static uint64 MetadataBytes() { return VoxelBoundedAllocation::WorkerSharedBytes<FStorage>(); }
	void BindLease(TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease) { Lease = MoveTemp(InLease); }
	void Reset() { Storage.Reset(); }
	void Reserve(int32 Count) { if (Count > 0) MutableValues().Reserve(Count); }
	void Add(const ElementType& Value) { MutableValues().Add(Value); }
	int32 Num() const { return Values().Num(); }
	bool IsEmpty() const { return Values().IsEmpty(); }
	uint64 GetAllocatedSize() const { return Values().GetAllocatedSize(); }
	const ElementType& operator[](int32 Index) const { return Values()[Index]; }
	bool Contains(const ElementType& Value) const { return Values().Contains(Value); }
	template<typename PredicateType>
	bool ContainsByPredicate(PredicateType Predicate) const { return Values().ContainsByPredicate(MoveTemp(Predicate)); }
	auto begin() const { return Values().begin(); }
	auto end() const { return Values().end(); }
};

struct WHFRAMEWORK_API FVoxelCoverageLocalSnapshot
{
	FGuid StorageId;
	uint64 StorageGeneration = 0;
	uint64 MembershipVersion = 0;
	bool bFullIndex = false;
	bool bIncludePresentation = true;
	TVoxelCoverageSnapshotArray<TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe>> Records;
	TVoxelCoverageSnapshotArray<FVoxelCoverageReadGuard> Guards;
	TVoxelCoverageSnapshotArray<FVoxelCoverageRecordReadGuard> RecordGuards;
	int32 VisitedIndexNodes = 0;
	int32 ExaminedRecords = 0;
	int32 MatchedRecords = 0;
	int32 GuardCount = 0;
	bool bCountOnly = false;
	bool bChangeSet = false;
	EVoxelTerrainResourceAdmission ResourceAdmission = EVoxelTerrainResourceAdmission::None;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	static uint64 ArrayMetadataBytes()
	{
		return decltype(Records)::MetadataBytes() + decltype(Guards)::MetadataBytes() + decltype(RecordGuards)::MetadataBytes();
	}
};

// 由 ViewManager 单一持有；工作线程只持有不可变 Records 与版本守卫。
class WHFRAMEWORK_API FVoxelCoverageIndex
{
public:
	static uint64 MaximumStorageBytes(int32 InRecordCount);
	void ReserveForRecords(int32 InRecordCount);
	void BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	void BindStorageLease(TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease);
	bool Upsert(TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe> InRecord, FString& OutError);
	bool Upsert(const FVoxelCoverageRecord& InRecord, FString& OutError);
	void MarkPublishedInputs() const;
	bool CanRemoveWithoutAllocation(const FVoxelTerrainSpatialKey& InKey) const;
	bool Remove(const FVoxelTerrainSpatialKey& InKey);
	bool Remove(const FVoxelTerrainSpatialKey& InKey, FString& OutError);
	TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe> Find(const FVoxelTerrainSpatialKey& InKey) const;
	bool HasMatchingRecord(const FVoxelCoverageRecord& InRecord) const;
	void Capture(const FIntVector& InMin, const FIntVector& InMax, FVoxelCoverageLocalSnapshot& OutSnapshot,
		bool bInIncludePresentation = true, bool bInCountOnly = false) const;
	// 计数不分配数组；调用方先授予容量，再按同一只读索引捕获。
	void CountCapture(const FIntVector& InMin, const FIntVector& InMax, FVoxelCoverageLocalSnapshot& OutCounts) const
	{
		Capture(InMin, InMax, OutCounts, false, true);
	}
	FVoxelGameThreadContinuation CaptureSteps(
		FIntVector InMin, FIntVector InMax, FVoxelCoverageLocalSnapshot& OutSnapshot, bool bInIncludePresentation = true) const;
	FVoxelGameThreadContinuation CaptureAllSteps(FVoxelCoverageLocalSnapshot& OutSnapshot, bool bInIncludePresentation = true) const;
	FVoxelGameThreadContinuation CaptureChangedSteps(const FVoxelCoverageIndex& InPrevious,
		FVoxelCoverageLocalSnapshot& OutSnapshot) const;
	bool IsCurrent(const FVoxelCoverageLocalSnapshot& InSnapshot) const;
	bool IsRangeCurrent(const FVoxelCoverageIndex& InFrozen, const FIntVector& InMin, const FIntVector& InMax,
		bool bInIncludePresentation = false) const;
	bool AnyRecord(const FIntVector& InMin, const FIntVector& InMax,
		TFunctionRef<bool(const FVoxelCoverageRecord&)> InPredicate) const;
	// 共享子树直接跳过；访问变化前后的记录，不创建临时 snapshot 数组。
	void VisitChangedRecords(const FVoxelCoverageIndex& InPrevious,
		TFunctionRef<void(const FVoxelCoverageRecord*, const FVoxelCoverageRecord*)> InVisitor) const;
	void Reset();
	void PruneEmptyNodes(int32 InOutstandingSnapshots);
	int32 NumRecords() const;
	int32 NumIndexNodes() const;
	bool HasResourceBinding() const { return Resources.IsValid(); }
	EVoxelTerrainResourceAdmission GetLastResourceAdmission() const { return LastResourceAdmission; }
	// 仅声明调用方额外固持且不在本索引中的输入；不授额，不包含本索引旧根／新路径。
	void SetAdditionalRequiredInput(uint64 InBytes, int32 InLeaseCount)
	{ AdditionalRequiredInputBytes = InBytes; AdditionalRequiredLeaseCount = InLeaseCount; }
	static bool AccumulateRetainedInputs(const FVoxelTerrainResources& InResources,
		TConstArrayView<const FVoxelCoverageIndex*> InIndexes, TConstArrayView<uint64> InExcludedSerials,
		uint64& InOutBytes, int32& InOutLeaseCount);
	// 同空间路径的旧新冻结根按 serial 去重；使用固定递归栈，不分配追踪表。
	bool AccumulateRetainedInput(const FVoxelTerrainResources& InResources, const FVoxelCoverageIndex* InOther,
		TConstArrayView<uint64> InExcludedSerials, uint64& InOutBytes, int32& InOutLeaseCount) const;

private:
	struct FNode
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		TSharedPtr<const FNode, ESPMode::ThreadSafe> Children[8];
		TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe> Records[4];
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RecordLeases[4];
		const FVoxelTerrainResources* AggregateLedger = nullptr;
		uint64 AggregateInputBytes = 0, MinimumSerial = MAX_uint64, MaximumSerial = 0;
		int32 AggregateLeaseCount = 0;
		uint64 RecordVersions[4][2] = {};
		uint64 ChildVersions[8][2] = {};
		uint64 OwnVersions[2] = {};
		uint64 SubtreeVersions[2] = {};
		int32 SubtreeRecords = 0;
		int32 SubtreeNodes = 1;
	};
	struct FRoot
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		// 完整 int32 世界划成 8 个 3D 半空间根和 4 个显式高度列根。
		TSharedPtr<const FNode, ESPMode::ThreadSafe> Nodes[12];
		uint64 Versions[12][2] = {};
		int32 Records = 0;
		int32 NodesCount = 0;
	};
	struct FQuery
	{
		int64 MinX = 0;
		int64 MinY = 0;
		int64 MinZ = 0;
		int64 MaxX = 0;
		int64 MaxY = 0;
		int64 MaxZ = 0;
		FIntVector ExactMin = FIntVector::ZeroValue;
		FIntVector ExactMax = FIntVector::ZeroValue;
	};
	using FNodePtr = TSharedPtr<const FNode, ESPMode::ThreadSafe>;
	static constexpr uint8 RootLevel = 27;
	static FVoxelCoverageIndexKey IndexKey(const FVoxelTerrainSpatialKey& InKey);
	static FVoxelCoverageIndexKey RootKey(int32 InSlot);
	static FVoxelCoverageIndexKey ChildKey(const FVoxelCoverageIndexKey& InKey, int32 InChild);
	static int32 RootSlot(const FVoxelCoverageIndexKey& InKey);
	static int32 ChildSlot(const FVoxelCoverageIndexKey& InTarget, uint8 InParentLevel);
	static FQuery Query(const FIntVector& InMin, const FIntVector& InMax);
	const FNode* FindNode(const FVoxelCoverageIndexKey& InKey, int32 InVersionKind, uint64& OutGapVersion) const;
	bool RemoveUnique(const FVoxelTerrainSpatialKey& InKey);
	void RefreshNode(FNode& InNode) const;
	bool Mutate(const FVoxelTerrainSpatialKey& InKey, const FVoxelCoverageRecord* InRecord,
		TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe> InSharedRecord, FString& OutError);
	FNodePtr MutateNode(FNodePtr InPrevious, const FVoxelCoverageIndexKey& InNodeKey,
		const FVoxelCoverageIndexKey& InTarget, int32 InRecordSlot,
		TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe> InRecord,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InRecordLease, bool bInInputsChanged,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InGrant);
	static void CaptureNode(FNodePtr InNode, FVoxelCoverageIndexKey InKey, const FQuery& InQuery,
		FVoxelCoverageLocalSnapshot& OutSnapshot, uint64 InGapVersion, bool bInCollect = false);
	static bool AnyNodeRecord(FNodePtr InNode, FVoxelCoverageIndexKey InKey, const FQuery& InQuery,
		TFunctionRef<bool(const FVoxelCoverageRecord&)> InPredicate);
	static bool CompareNodeRange(FNodePtr InCurrent, FNodePtr InFrozen, FVoxelCoverageIndexKey InKey,
		const FQuery& InQuery, int32 InVersionKind, uint64 InCurrentGap, uint64 InFrozenGap);
	static FVoxelGameThreadContinuation CaptureNodeSteps(FNodePtr InNode, FVoxelCoverageIndexKey InKey,
		FQuery InQuery, FVoxelCoverageLocalSnapshot& OutSnapshot, uint64 InGapVersion, bool bInCollect = false);
	static FVoxelGameThreadContinuation CaptureFrozenSteps(FVoxelCoverageIndex InFrozen, FQuery InQuery,
		FVoxelCoverageLocalSnapshot& OutSnapshot, bool bInIncludePresentation, bool bInFullIndex);
	static FVoxelGameThreadContinuation CaptureChangedNodeSteps(FNodePtr InCurrent, FNodePtr InPrevious,
		FVoxelCoverageLocalSnapshot& OutSnapshot);
	static FVoxelGameThreadContinuation CaptureChangedFrozenSteps(FVoxelCoverageIndex InFrozen, FVoxelCoverageIndex InPrevious,
		FVoxelCoverageLocalSnapshot& OutSnapshot);
	bool ReserveSnapshot(FVoxelCoverageLocalSnapshot& InOutSnapshot) const;
	TSharedPtr<const FRoot, ESPMode::ThreadSafe> Root;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> StorageLease;
	FVoxelGenerationConsumerKey Consumer;
	EVoxelTerrainResourceAdmission LastResourceAdmission = EVoxelTerrainResourceAdmission::None;
	uint64 AdditionalRequiredInputBytes = 0;
	int32 AdditionalRequiredLeaseCount = 0;
	uint64 Serial = 0;
	uint64 MembershipVersion = 0;
	uint64 StorageGeneration = 1;
	FGuid StorageId = FGuid::NewGuid();
	int32 ReservedRecordLimit = 0;
};
