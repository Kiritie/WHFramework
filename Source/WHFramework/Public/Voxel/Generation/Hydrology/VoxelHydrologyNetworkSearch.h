#pragma once

#include "Voxel/Generation/Hydrology/VoxelHydrologyRegionGraph.h"
#include "Voxel/Generation/VoxelGenerationPagedArray.h"

// 证明请求的种子是宏观格坐标；整区与单点请求不能混成同一个缓存身份。
struct WHFRAMEWORK_API FVoxelHydrologyNetworkKey
{
	FIntPoint Seed = FIntPoint::ZeroValue;
	bool bEntireRegion = false;
	bool operator==(const FVoxelHydrologyNetworkKey& InOther) const = default;
	friend uint32 GetTypeHash(const FVoxelHydrologyNetworkKey& InKey)
	{
		return HashCombine(GetTypeHash(InKey.Seed), GetTypeHash(InKey.bEntireRegion));
	}
};

struct WHFRAMEWORK_API FVoxelHydrologyNetworkCell
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	FVoxelHydrologyRegionPotential Potential;
	FIntPoint ParentCell = FIntPoint::ZeroValue;
	bool bHasParent = false;
	int32 ReceiverFloorZ = MIN_int32;
	bool operator==(const FVoxelHydrologyNetworkCell& InOther) const = default;
};

struct WHFRAMEWORK_API FVoxelHydrologyNetworkResult
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	uint64 Generation = 0;
	uint64 InputFingerprint = 0;
	uint64 Fingerprint = 0;
	int32 Threshold = MAX_int32;
	TArray<FVoxelHydrologyNetworkCell> Cells;

	const FVoxelHydrologyNetworkCell* Find(const FIntPoint& InCell) const;
	uint64 GetAllocatedBytes() const;
	bool HasEquivalentOutput(const FVoxelHydrologyNetworkResult& InOther) const;
};

enum class EVoxelHydrologyNetworkStep : uint8
{
	None,
	Yielded,
	NeedInput,
	CapacityBlocked,
	Ready,
	NoReceiver,
	Canceled,
	Failed
};

// 只在冻结区域摘要的关键节点上证明水位和父方向；未知边界不作为出口。
class WHFRAMEWORK_API FVoxelHydrologyNetworkSearch
{
public:
	// 读取者的原租约须保证返回的冻结区域图在本次 Advance 内有效。
	using FReadRegion = TFunctionRef<EVoxelHydrologyClosureRead(const FIntPoint&, const FVoxelHydrologyRegionGraph*&, FString&)>;

	FVoxelHydrologyNetworkSearch();
	~FVoxelHydrologyNetworkSearch();
	FVoxelHydrologyNetworkSearch(const FVoxelHydrologyNetworkSearch&) = delete;
	FVoxelHydrologyNetworkSearch& operator=(const FVoxelHydrologyNetworkSearch&) = delete;
	bool Initialize(const FIntPoint& InSeedNode, int32 InCellSize, int32 InRegionSide, uint64 InGeneration, FString& OutError);
	// 汇水正式需要整区控制，在同一证明工作集中闭合全部本区节点，避免逐节点重启。
	bool InitializeRegion(const FIntPoint& InSeedNode, int32 InCellSize, int32 InRegionSide, uint64 InGeneration, FString& OutError);
	// 可选绝对截止点每 32 个微步骤检查；0 只使用工作量上限，等待输入仍立即返回。
	EVoxelHydrologyNetworkStep Advance(uint64 InGeneration, FReadRegion InReadRegion, int32 InWorkBudget,
		FString& OutError, const TAtomic<bool>* InCancel = nullptr, double InDeadlineSeconds = 0.0);
	const FVoxelHydrologyNetworkResult& GetResult() const;
	FVoxelHydrologyNetworkResult TakeResult();
	uint64 GetAllocatedBytes() const;
	int32 GetNodeCount() const;
	int32 GetRegionCount() const;

private:
	enum class EStage : uint8
	{
		None,
		LoadRegion,
		ResetSolve,
		SpillSeeds,
		Spill,
		Anchors,
		Plateaus,
		Parents,
		SaveLower,
		Certify,
		TraceBoundary,
		ReceiverProof,
		ReceiverValidate,
		Emit,
		Sort,
		Hash,
		Ready,
		Taken
	};

	struct FRegion
	{
		FIntPoint Coordinate = FIntPoint::ZeroValue;
		uint64 InputFingerprint = 0;
		uint64 Fingerprint = 0;
		const FVoxelHydrologyRegionGraph* SliceGraph = nullptr;
	};

	struct FNode
	{
		FIntPoint Cell = FIntPoint::ZeroValue;
		FIntPoint ParentCell = FIntPoint::ZeroValue;
		int32 ParentIndex = INDEX_NONE;
		int32 ReceiverFloorZ = MIN_int32;
		int32 ReceiverBlocker = INDEX_NONE;
		int32 ReceiverCaller = INDEX_NONE;
		uint8 ReceiverState = 0;
		int32 Region = INDEX_NONE;
		int32 LocalIndex = INDEX_NONE;
		int32 GroundZ = 0;
		int32 WaterZ = MIN_int32;
		FVoxelHydrologyRegionPotential Potential;
		FVoxelHydrologyRegionPotential LowerPotential;
		FIntPoint LowerParentCell = FIntPoint::ZeroValue;
		uint8 UnknownFaces = 0;
		bool bIncluded = false;
		bool bHasParent = false;
		bool bLowerHasParent = false;
		bool IsReceiver() const { return WaterZ != MIN_int32; }
	};

	struct FConnection
	{
		int32 Neighbor = INDEX_NONE;
		int32 BarrierZ = 0;
		uint32 Steps = 0;
		uint32 LastBarrierDistance = 0;
		FIntPoint FirstStep = FIntPoint::ZeroValue;
		int32 Edge = INDEX_NONE;
		bool bFromFirst = false;
		bool bAtEnd = false;
	};

	struct FEvent
	{
		int32 Index = INDEX_NONE;
		int32 Plane = 0;
		int64 Distance = 0;
	};

	EVoxelHydrologyClosureRead ReadRegion(const FIntPoint& InCoordinate, FReadRegion InRead,
		int32& OutRegion, const FVoxelHydrologyRegionGraph*& OutGraph, FString& OutError);
	EVoxelHydrologyClosureRead ReadKnownRegion(int32 InRegion, FReadRegion InRead,
		const FVoxelHydrologyRegionGraph*& OutGraph, FString& OutError);
	bool AppendNode(int32 InRegion, int32 InLocalIndex, const FVoxelHydrologyRegionGraph& InGraph, int32& OutIndex, FString& OutError);
	EVoxelHydrologyClosureRead ReadConnection(int32 InNode, int32 InSlot, FReadRegion InRead,
		FConnection& OutConnection, FString& OutError);
	EVoxelHydrologyClosureRead ResolveNeighborPotential(int32 InNode, const FConnection& InConnection, FReadRegion InRead,
		FVoxelHydrologyRegionPotential& OutPotential, FString& OutError);
	bool EventLess(const FEvent& InFirst, const FEvent& InSecond) const;
	bool IsSolveRoot(const FNode& InNode) const;
	bool Push(const FEvent& InEvent, FString& OutError);
	FEvent Pop();
	void SiftResult(int32 InRoot, int32 InEnd);
	void FreeScratch();
	EVoxelHydrologyNetworkStep Failure(FString& OutError) const;
	EVoxelHydrologyNetworkStep ReadFailure(EVoxelHydrologyClosureRead InRead, FString& OutError) const;

	FVoxelHydrologyNetworkResult Result;
	TVoxelGenerationPagedArray<FNode> Nodes;
	TArray<FRegion> Regions;
	TMap<FIntPoint, int32> ByCell;
	TMap<FIntPoint, int32> ByRegion;
	TArray<FEvent> Frontier;
	FIntPoint Seed = FIntPoint::ZeroValue;
	FIntPoint PendingRegion = FIntPoint::ZeroValue;
	FIntPoint Minimum = FIntPoint::ZeroValue;
	FIntPoint Maximum = FIntPoint::ZeroValue;
	int32 CellSize = 0;
	int32 RegionSide = 0;
	int32 CellMapCapacity = 0;
	int32 RegionMapCapacity = 0;
	int32 IncludedCount = 0;
	int32 LoadCursor = 0;
	int32 TargetIndex = INDEX_NONE;
	int32 TraceSteps = 0;
	int32 DirectionProofExpansions = 0;
	int32 ReceiverProofExpansions = 0;
	uint64 RegionReadRequests = 0;
	uint64 SliceGraphHits = 0;
	bool bSolvingLower = true;
	bool bEntireRegion = false;
	int32 Cursor = 0;
	int32 Slot = 0;
	int32 Current = INDEX_NONE;
	int32 SortRoot = INDEX_NONE;
	int32 SortEnd = INDEX_NONE;
	FEvent ActiveEvent;
	EStage Stage = EStage::None;
};
