#pragma once

#include "Voxel/Generation/Hydrology/VoxelHydrologyNetworkRunoff.h"

struct WHFRAMEWORK_API FVoxelHydrologyLocalCell
{
	FVoxelHydrologyRegionSample Sample;
	FVoxelHydrologyRegionPotential Potential;
	FIntPoint ParentCell = FIntPoint::ZeroValue;
	uint64 Accumulation = 0;
	bool bHasParent = false;
	int32 ChannelWaterZ = MAX_int32;
	int32 ReceiverFloorZ = MIN_int32;
};

struct WHFRAMEWORK_API FVoxelHydrologyLocalRunoff
{
	FIntPoint Minimum = FIntPoint::ZeroValue;
	int32 Width = 0;
	int32 Height = 0;
	int32 CellSize = 0;
	uint64 Generation = 0;
	uint64 GraphFingerprint = 0;
	uint64 Fingerprint = 0;
	uint64 LocalRainWeight = 0;
	uint64 IncomingRainWeight = 0;
	uint64 ReceivedRainWeight = 0;
	uint64 ExportedRainWeight = 0;
	TArray<FVoxelHydrologyLocalCell> Cells;

	const FVoxelHydrologyLocalCell* Find(const FIntPoint& InCell) const;
	uint64 GetAllocatedBytes() const;
};

// 仅为实际需求区域重建同一森林，接纳冻结边界方向及真实入境边来水。
class WHFRAMEWORK_API FVoxelHydrologyLocalRunoffBuild
{
public:
	using FReadBoundary = TFunctionRef<EVoxelHydrologyClosureRead(const FIntPoint&,
		FVoxelHydrologyNetworkCell&, uint64&, FString&)>;

	FVoxelHydrologyLocalRunoffBuild();
	~FVoxelHydrologyLocalRunoffBuild();
	FVoxelHydrologyLocalRunoffBuild(const FVoxelHydrologyLocalRunoffBuild&) = delete;
	FVoxelHydrologyLocalRunoffBuild& operator=(const FVoxelHydrologyLocalRunoffBuild&) = delete;
	bool Initialize(const FVoxelHydrologyRegionGraph& InGraph, FString& OutError);
	EVoxelHydrologyRegionStep Advance(const FVoxelHydrologyRegionGraph& InGraph,
		FVoxelHydrologyRegionGraphBuild::FReadCell InReadCell, FReadBoundary InReadBoundary,
		int32 InWorkBudget, FString& OutError, const TAtomic<bool>* InCancel = nullptr);
	EVoxelHydrologyRegionStep Advance(const FVoxelHydrologyRegionGraph& InGraph,
		const FVoxelHydrologyNetworkRunoff& InRunoff, FVoxelHydrologyRegionGraphBuild::FReadCell InReadCell,
		int32 InWorkBudget, FString& OutError, const TAtomic<bool>* InCancel = nullptr);
	const FVoxelHydrologyLocalRunoff& GetResult() const;
	FVoxelHydrologyLocalRunoff TakeResult();
	uint64 GetAllocatedBytes() const;

private:
	enum class EStage : uint8
	{
		None,
		Allocate,
		Cells,
		Boundary,
		SpillSeeds,
		Spill,
		Anchors,
		Plateaus,
		Parents,
		ValidateControls,
		Children,
		Queue,
		Accumulate,
		Hash,
		Ready,
		Taken
	};

	struct FState
	{
		int32 Children = 0;
		int32 Control = INDEX_NONE;
	};
	struct FBoundary
	{
		int32 Index = INDEX_NONE;
		FVoxelHydrologyNetworkCell Control;
	};
	struct FEvent
	{
		int32 Index = INDEX_NONE;
		int32 Plane = 0;
		int64 Distance = 0;
	};

	FIntPoint Position(int32 InIndex) const;
	int32 IndexOf(const FIntPoint& InCell) const;
	bool IsBoundary(int32 InIndex) const;
	bool EventLess(const FEvent& InFirst, const FEvent& InSecond) const;
	bool Push(const FEvent& InEvent, FString& OutError);
	FEvent Pop();
	void FreeScratch();
	EVoxelHydrologyRegionStep Failure(FString& OutError) const;

	FVoxelHydrologyLocalRunoff Result;
	TArray<FState> States;
	TArray<FBoundary> Boundaries;
	TArray<FEvent> Frontier;
	TArray<int32> Queue;
	FEvent ActiveEvent;
	EStage Stage = EStage::None;
	uint64 InputFingerprint = 0;
	uint64 CapturedInputFingerprint = 0;
	int32 CellCount = 0;
	int32 Cursor = 0;
	int32 Current = INDEX_NONE;
	int32 Face = 0;
};
