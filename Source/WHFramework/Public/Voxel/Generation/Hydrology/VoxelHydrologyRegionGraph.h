#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyMacroInput.h"

class FVoxelTerrainResourceLease;

struct WHFRAMEWORK_API FVoxelHydrologyRegionNode
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	int32 GroundZ = 0;
	int32 WaterZ = MIN_int32;
	FVoxelStableId WaterId;
	uint64 RainWeight = 0;
	uint8 BoundaryFaces = 0;
	uint8 EdgeCount = 0;
	int32 Edges[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};

	bool IsReceiver() const { return WaterZ != MIN_int32; }
	bool operator==(const FVoxelHydrologyRegionNode& InOther) const = default;
};

struct WHFRAMEWORK_API FVoxelHydrologyRegionEdge
{
	int32 First = INDEX_NONE;
	int32 Second = INDEX_NONE;
	int32 BarrierZ = 0;
	uint32 CellSteps = 0;
	uint64 InteriorRainWeight = 0;
	int32 ProfileOffset = 0;
	int32 ProfileCount = 0;
	FIntPoint FirstStep = FIntPoint::ZeroValue;
	FIntPoint LastStep = FIntPoint::ZeroValue;
	bool operator==(const FVoxelHydrologyRegionEdge& InOther) const = default;
};

struct WHFRAMEWORK_API FVoxelHydrologyRegionProfile
{
	uint32 CellIndex = 0;
	int32 GroundZ = 0;
	int32 FirstBarrierZ = MIN_int32;
	int32 SecondBarrierZ = MIN_int32;
	uint64 RainPrefix = 0;
	bool operator==(const FVoxelHydrologyRegionProfile& InOther) const = default;
};

struct WHFRAMEWORK_API FVoxelHydrologyRegionPotential
{
	int32 SpillZ = MAX_int32;
	int64 PlateauDistance = MAX_int64;
	bool operator==(const FVoxelHydrologyRegionPotential& InOther) const = default;
};

struct WHFRAMEWORK_API FVoxelHydrologyRegionRoute
{
	FVoxelHydrologyRegionPotential Potential;
	FIntPoint ParentCell = FIntPoint::ZeroValue;
	bool bTowardFirst = false;
};

struct WHFRAMEWORK_API FVoxelHydrologyRegionSplit
{
	int32 FirstProfileCount = 0;
	uint64 FirstRainWeight = 0;
	uint64 SecondRainWeight = 0;
};

struct WHFRAMEWORK_API FVoxelHydrologyRegionAnchorRun
{
	uint32 EndExclusive = 0;
	int32 Node = INDEX_NONE;
	bool operator==(const FVoxelHydrologyRegionAnchorRun& InOther) const = default;
};

enum class EVoxelHydrologyRegionCellKind : uint8
{
	None,
	Node,
	Profile,
	Pruned,
	Wet
};

struct WHFRAMEWORK_API FVoxelHydrologyRegionPrunedCell
{
	uint32 Parent = 0;
	uint32 CellIndex = 0;
	uint64 RainWeight = 0;
	bool operator==(const FVoxelHydrologyRegionPrunedCell& InOther) const = default;
};

// 边界口、真实湿根和分叉构成摘要；边内雨量在全局方向确定前不擅自分配。
struct WHFRAMEWORK_API FVoxelHydrologyRegionGraph
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FIntPoint Minimum = FIntPoint::ZeroValue;
	int32 Width = 0;
	int32 Height = 0;
	int32 CellSize = 0;
	uint64 Generation = 0;
	uint64 InputFingerprint = 0;
	uint64 Fingerprint = 0;
	uint64 TotalLandRainWeight = 0;
	TArray<FVoxelHydrologyRegionNode> Nodes;
	TArray<FVoxelHydrologyRegionEdge> Edges;
	TArray<FVoxelHydrologyRegionProfile> Profiles;
	TArray<FVoxelHydrologyRegionAnchorRun> AnchorRuns;
	TArray<uint8> ForestNeighbors;
	TArray<uint32> CellLocations;
	TArray<FVoxelHydrologyRegionPrunedCell> PrunedCells;
	TArray<int32> PrunedByParent;

	uint64 GetAllocatedBytes() const;
	bool ValidateCore(const FIntPoint& InCoordinate, int32 InCellSize, int32 InRegionSide, uint64 InGeneration, FString& OutError) const;
	bool HasEquivalentOutput(const FVoxelHydrologyRegionGraph& InOther) const;
	FIntPoint GetProfileCell(int32 InIndex) const;
	int32 FindComponentAnchor(const FIntPoint& InCell) const;
	int32 FindNodeIndex(const FIntPoint& InCell) const;
	EVoxelHydrologyRegionCellKind FindCellLocation(const FIntPoint& InCell, int32& OutIndex) const;
	int32 FindProfileEdge(int32 InProfile) const;
	uint64 GetMaxPrunedChildRainWeight(uint32 InCellIndex) const;
	uint32 GetLastBarrierDistance(int32 InEdge, bool bFromFirst) const;
	bool ResolveProfile(int32 InEdge, int32 InOffset, const FVoxelHydrologyRegionPotential& InFirst,
		const FVoxelHydrologyRegionPotential& InSecond, FVoxelHydrologyRegionRoute& OutRoute, FString& OutError) const;
	bool SplitEdgeRain(int32 InEdge, const FVoxelHydrologyRegionPotential& InFirst,
		const FVoxelHydrologyRegionPotential& InSecond, FVoxelHydrologyRegionSplit& OutSplit, FString& OutError) const;

private:
	bool ValidatePotentials(int32 InEdge, const FVoxelHydrologyRegionPotential& InFirst,
		const FVoxelHydrologyRegionPotential& InSecond, FString& OutError) const;
	FVoxelHydrologyRegionRoute ResolveProfileUnchecked(int32 InEdge, int32 InOffset,
		const FVoxelHydrologyRegionPotential& InFirst, const FVoxelHydrologyRegionPotential& InSecond) const;
};

enum class EVoxelHydrologyRegionStep : uint8
{
	None,
	Yielded,
	NeedInput,
	Ready,
	CapacityBlocked,
	Canceled,
	Failed
};

// 原生工作状态由调用者原授额持有；读取、排序、收缩与结果校验均支持续跑。
class WHFRAMEWORK_API FVoxelHydrologyRegionGraphBuild
{
public:
	using FReadCell = TFunctionRef<EVoxelHydrologyClosureRead(const FIntPoint&, FVoxelHydrologyRegionSample&, FString&)>;

	FVoxelHydrologyRegionGraphBuild();
	~FVoxelHydrologyRegionGraphBuild();
	FVoxelHydrologyRegionGraphBuild(const FVoxelHydrologyRegionGraphBuild&) = delete;
	FVoxelHydrologyRegionGraphBuild& operator=(const FVoxelHydrologyRegionGraphBuild&) = delete;
	bool Initialize(const FIntPoint& InMinimum, int32 InWidth, int32 InHeight, int32 InCellSize, uint64 InGeneration, FString& OutError);
	EVoxelHydrologyRegionStep Advance(uint64 InGeneration, FReadCell InReadCell, int32 InWorkBudget, FString& OutError, const TAtomic<bool>* InCancel = nullptr);
	bool IsReady() const;
	const FVoxelHydrologyRegionGraph& GetResult() const;
	FVoxelHydrologyRegionGraph TakeResult();
	uint64 GetAllocatedBytes() const;

private:
	enum class EStage : uint8
	{
		None,
		Allocate,
		Read,
		Candidates,
		Sort,
		Forest,
		Anchors,
		Links,
		Leaves,
		Peel,
		CountNodes,
		Nodes,
		CountAnchors,
		CellAnchors,
		Edges,
		Profiles,
		CheckNodes,
		CheckProfiles,
		CheckEdges,
		CheckAnchors,
		SortPruned,
		CheckPruned,
		Ready,
		Taken
	};

	struct FCell
	{
		FVoxelHydrologyRegionSample Sample;
		uint64 RainWeight = 0;
		int32 Parent = INDEX_NONE;
		int32 Head = INDEX_NONE;
		int32 Degree = 0;
		int32 Node = INDEX_NONE;
		uint8 Rank = 0;
		bool bRemoved = false;
		bool bQueued = false;
		bool bProfiled = false;
		bool bAnchor = false;
	};

	struct FCandidate
	{
		int32 First = INDEX_NONE;
		int32 Second = INDEX_NONE;
		int32 BarrierZ = 0;
		bool bKept = false;
	};

	struct FLink
	{
		int32 Neighbor = INDEX_NONE;
		int32 BarrierZ = 0;
		int32 Next = INDEX_NONE;
	};

	bool Allocate(FString& OutError);
	FIntPoint Position(int32 InIndex) const;
	uint8 BoundaryFaces(int32 InIndex) const;
	bool IsTerminal(int32 InIndex) const;
	bool KeepNode(int32 InIndex) const;
	bool CandidateLess(const FCandidate& InLeft, const FCandidate& InRight) const;
	void Sift(int32 InRoot, int32 InEnd);
	void SiftPruned(int32 InRoot, int32 InEnd);
	int32 FindRoot(int32 InIndex);
	void QueueLeaf(int32 InIndex);
	void FreeScratch();
	void AddFingerprint(uint64 InValue);
	EVoxelHydrologyRegionStep Failure(FString& OutError) const;

	FVoxelHydrologyRegionGraph Result;
	TArray<FCell> Cells;
	TArray<FCandidate> Candidates;
	TArray<FLink> Links;
	TArray<int32> Leaves;
	EStage Stage = EStage::None;
	int32 CellCount = 0;
	int32 Cursor = 0;
	int32 NodeCount = 0;
	int32 SummaryHalfEdges = 0;
	int32 InteriorCells = 0;
	int32 AnchorCount = 0;
	int32 PreviousAnchor = MAX_int32;
	int32 KeptEdges = 0;
	int32 SortRoot = INDEX_NONE;
	int32 SortEnd = INDEX_NONE;
	int32 LinkCursor = INDEX_NONE;
	int32 WalkStart = INDEX_NONE;
	int32 WalkPrevious = INDEX_NONE;
	int32 WalkCurrent = INDEX_NONE;
	int32 WalkFirst = INDEX_NONE;
	int32 WalkBarrierZ = MIN_int32;
	uint32 WalkSteps = 0;
	uint64 WalkRainWeight = 0;
	uint64 CheckedRainWeight = 0;
	int32 TraceEdge = INDEX_NONE;
	int32 TracePrevious = INDEX_NONE;
	int32 TraceCurrent = INDEX_NONE;
	int32 TraceStop = INDEX_NONE;
	int32 TraceBarrierZ = MIN_int32;
	uint64 TraceRainWeight = 0;
	int32 SuffixCursor = 0;
	int32 SuffixBarrierZ = MIN_int32;
	bool bSuffixActive = false;
};
