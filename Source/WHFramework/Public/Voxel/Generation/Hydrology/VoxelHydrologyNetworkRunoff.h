#pragma once

#include "Voxel/Generation/Hydrology/VoxelHydrologyNetworkSearch.h"

struct WHFRAMEWORK_API FVoxelHydrologyNetworkRunoffCell
{
	FVoxelHydrologyNetworkCell Control;
	uint64 Accumulation = 0;
	uint64 IncomingRainWeight = 0;
	uint64 MaxChildRainWeight = 0;
};

struct WHFRAMEWORK_API FVoxelHydrologySourceCell
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	FIntPoint ParentCell = FIntPoint::ZeroValue;
	uint64 Accumulation = 0;
	uint64 MaxChildRainWeight = 0;
	bool bReceiver = false;
	bool bHasParent = false;

	bool IsQualified(uint64 InThreshold) const;
};

struct WHFRAMEWORK_API FVoxelHydrologyNetworkRunoff
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	uint64 Generation = 0;
	uint64 GraphFingerprint = 0;
	uint64 InputFingerprint = 0;
	uint64 Fingerprint = 0;
	TArray<FVoxelHydrologyNetworkRunoffCell> Cells;

	const FVoxelHydrologyNetworkRunoffCell* Find(const FIntPoint& InCell) const;
	uint64 GetAllocatedBytes() const;
	bool SampleSourceCell(const FVoxelHydrologyRegionGraph& InGraph, const FIntPoint& InCell,
		FVoxelHydrologySourceCell& OutCell, FString& OutError) const;
};

// 在摘要节点上闭合上游依赖，边内雨量按真实分水分配，入境量只来自境外父子边。
class WHFRAMEWORK_API FVoxelHydrologyNetworkRunoffBuild
{
public:
	using FReadControl = TFunctionRef<EVoxelHydrologyClosureRead(const FIntPoint&, FVoxelHydrologyNetworkCell&, FString&)>;
	using FReadRegion = FVoxelHydrologyNetworkSearch::FReadRegion;

	FVoxelHydrologyNetworkRunoffBuild();
	~FVoxelHydrologyNetworkRunoffBuild();
	FVoxelHydrologyNetworkRunoffBuild(const FVoxelHydrologyNetworkRunoffBuild&) = delete;
	FVoxelHydrologyNetworkRunoffBuild& operator=(const FVoxelHydrologyNetworkRunoffBuild&) = delete;
	bool Initialize(const FIntPoint& InCoordinate, int32 InCellSize, int32 InRegionSide, uint64 InGeneration, FString& OutError);
	// 截止点贯穿汇水、嵌套控制证明及复制，不因跨阶段重新获得时间片。
	EVoxelHydrologyRegionStep Advance(uint64 InGeneration, FReadRegion InReadRegion, FReadControl InReadControl,
		int32 InWorkBudget, FString& OutError, const TAtomic<bool>* InCancel = nullptr, double InDeadlineSeconds = 0.0);
	const FVoxelHydrologyNetworkRunoff& GetResult() const;
	FVoxelHydrologyNetworkRunoff TakeResult();
	uint64 GetAllocatedBytes() const;
	int32 GetNodeCount() const;

private:
	enum class EStage : uint8
	{
		None,
		Seeds,
		Solve,
		Hash,
		Ready,
		Taken
	};
	enum class ENodeState : uint8
	{
		None,
		Visiting,
		Done
	};
	struct FNode
	{
		FIntPoint Cell = FIntPoint::ZeroValue;
		FVoxelHydrologyNetworkCell Control;
		uint64 GraphFingerprint = 0;
		uint64 Accumulation = 0;
		uint64 IncomingRainWeight = 0;
		uint64 MaxChildRainWeight = 0;
		uint64 ChildLocalRainWeight = 0;
		int32 LocalIndex = INDEX_NONE;
		int32 Slot = 0;
		int32 Child = INDEX_NONE;
		ENodeState State = ENodeState::None;
		bool bExternalChild = false;
		bool bControlReady = false;
	};
	struct FContribution
	{
		FIntPoint Child = FIntPoint::ZeroValue;
		uint64 RainWeight = 0;
		uint64 MaxChildRainWeight = 0;
		uint64 InputFingerprint = 0;
		bool bHasChild = false;
		bool bExternal = false;
		bool bAtEnd = false;
	};

	FIntPoint RegionFor(const FIntPoint& InCell) const;
	bool IsPhysical(const FIntPoint& InCell) const;
	EVoxelHydrologyClosureRead ReadGraph(const FIntPoint& InCoordinate, FReadRegion InRead,
		const FVoxelHydrologyRegionGraph*& OutGraph, FString& OutError) const;
	EVoxelHydrologyClosureRead ReadControl(const FIntPoint& InCell, FReadRegion InReadRegion, FReadControl InReadControl,
		FVoxelHydrologyNetworkCell& OutControl, uint64& OutGraphFingerprint, FString& OutError);
	EVoxelHydrologyClosureRead ReadContribution(int32 InNode, FReadRegion InReadRegion, FReadControl InReadControl,
		FContribution& OutContribution, FString& OutError);
	bool AppendNode(const FIntPoint& InCell, int32& OutIndex, FString& OutError);
	bool ReserveStack(FString& OutError);
	void FreeScratch();
	EVoxelHydrologyRegionStep Failure(FString& OutError) const;
	EVoxelHydrologyRegionStep ReadFailure(EVoxelHydrologyClosureRead InRead, FString& OutError) const;

	FVoxelHydrologyNetworkRunoff Result;
	TVoxelGenerationPagedArray<FNode> Nodes;
	TMap<FIntPoint, int32> ByCell;
	TArray<int32> Stack;
	FIntPoint Minimum = FIntPoint::ZeroValue;
	FIntPoint Maximum = FIntPoint::ZeroValue;
	EStage Stage = EStage::None;
	int32 CellSize = 0;
	int32 RegionSide = 0;
	int32 MapCapacity = 0;
	int32 SeedCursor = 0;
	int32 Root = INDEX_NONE;
	int32 HashCursor = 0;
};
