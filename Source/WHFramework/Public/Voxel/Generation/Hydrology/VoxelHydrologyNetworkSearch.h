#pragma once

#include "Voxel/Generation/Hydrology/VoxelHydrologyRegionGraph.h"
#include "Voxel/Generation/VoxelGenerationPagedArray.h"

struct WHFRAMEWORK_API FVoxelHydrologyNetworkCell
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	FVoxelHydrologyRegionPotential Potential;
	FIntPoint ParentCell = FIntPoint::ZeroValue;
	bool bHasParent = false;
	bool operator==(const FVoxelHydrologyNetworkCell& InOther) const = default;
};

struct WHFRAMEWORK_API FVoxelHydrologyNetworkResult
{
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
	using FReadRegion = TFunctionRef<EVoxelHydrologyClosureRead(const FIntPoint&, const FVoxelHydrologyRegionGraph*&, FString&)>;

	FVoxelHydrologyNetworkSearch();
	~FVoxelHydrologyNetworkSearch();
	FVoxelHydrologyNetworkSearch(const FVoxelHydrologyNetworkSearch&) = delete;
	FVoxelHydrologyNetworkSearch& operator=(const FVoxelHydrologyNetworkSearch&) = delete;
	bool Initialize(const FIntPoint& InSeedNode, int32 InCellSize, int32 InRegionSide, uint64 InGeneration, FString& OutError);
	// 汇水正式需要整区控制，在同一证明工作集中闭合全部本区节点，避免逐节点重启。
	bool InitializeRegion(const FIntPoint& InSeedNode, int32 InCellSize, int32 InRegionSide, uint64 InGeneration, FString& OutError);
	EVoxelHydrologyNetworkStep Advance(uint64 InGeneration, FReadRegion InReadRegion, int32 InWorkBudget,
		FString& OutError, const TAtomic<bool>* InCancel = nullptr);
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
	};

	struct FNode
	{
		FIntPoint Cell = FIntPoint::ZeroValue;
		FIntPoint ParentCell = FIntPoint::ZeroValue;
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
