#pragma once

#include "Voxel/Generation/Hydrology/VoxelHydrologySourcePlan.h"

struct WHFRAMEWORK_API FVoxelHydrologyChannelCell
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	bool bActive = false;
	bool bIncomingActive = false;
	int32 WaterZ = MAX_int32;
	int32 ReceiverFloorZ = MIN_int32;
};

struct WHFRAMEWORK_API FVoxelHydrologyChannelPlan
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	uint64 Generation = 0;
	uint64 GraphFingerprint = 0;
	uint64 RunoffFingerprint = 0;
	uint64 SourceFingerprint = 0;
	uint64 Fingerprint = 0;
	TArray<FVoxelHydrologyChannelCell> Cells;

	const FVoxelHydrologyChannelCell* Find(const FIntPoint& InCell) const;
	uint64 GetAllocatedBytes() const;
};

struct WHFRAMEWORK_API FVoxelHydrologyChannelInputs
{
	const FVoxelHydrologyRegionGraph* Graph = nullptr;
	const FVoxelHydrologyNetworkRunoff* Runoff = nullptr;
	const FVoxelHydrologySourcePlan* Sources = nullptr;
};

// 只传播全局间距桶已选中的源头，水量达标本身不能伪造入境河道。
class WHFRAMEWORK_API FVoxelHydrologyChannelPlanBuild
{
public:
	using FReadRegion = TFunctionRef<EVoxelHydrologyClosureRead(const FIntPoint&, FVoxelHydrologyChannelInputs&, FString&)>;

	FVoxelHydrologyChannelPlanBuild();
	~FVoxelHydrologyChannelPlanBuild();
	bool Initialize(const FIntPoint& InCoordinate, int32 InCellSize, int32 InRegionSide, uint64 InGeneration, FString& OutError);
	EVoxelHydrologyRegionStep Advance(uint64 InGeneration, FReadRegion InReadRegion, int32 InWorkBudget,
		FString& OutError, const TAtomic<bool>* InCancel = nullptr);
	const FVoxelHydrologyChannelPlan& GetResult() const;
	FVoxelHydrologyChannelPlan TakeResult();
	uint64 GetAllocatedBytes() const;

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
		uint64 RunoffFingerprint = 0;
		uint64 SourceFingerprint = 0;
		int32 LocalIndex = INDEX_NONE;
		int32 Slot = 0;
		int32 Child = INDEX_NONE;
		ENodeState State = ENodeState::None;
		bool bActive = false;
		bool bIncomingActive = false;
		bool bExternalChild = false;
		int32 WaterZ = MAX_int32;
		int32 GroundCap = MAX_int32;
		int32 ReceiverWaterZ = MIN_int32;
		int32 ChildWaterCap = MAX_int32;
		int32 ReceiverFloorZ = MIN_int32;
	};
	struct FContribution
	{
		FIntPoint Child = FIntPoint::ZeroValue;
		uint64 Fingerprint = 0;
		bool bActive = false;
		bool bHasChild = false;
		bool bExternal = false;
		bool bAtEnd = false;
		int32 WaterCap = MAX_int32;
		int32 ChildWaterCap = MAX_int32;
	};

	FIntPoint RegionFor(const FIntPoint& InCell) const;
	EVoxelHydrologyClosureRead ReadInputs(const FIntPoint& InCoordinate, FReadRegion InReadRegion,
		FVoxelHydrologyChannelInputs& OutInputs, FString& OutError) const;
	EVoxelHydrologyClosureRead ReadContribution(int32 InNode, FReadRegion InReadRegion, FContribution& OutContribution, FString& OutError) const;
	bool AppendNode(const FIntPoint& InCell, int32& OutIndex, FString& OutError);
	bool ReserveStack(FString& OutError);
	EVoxelHydrologyRegionStep Failure(FString& OutError) const;
	EVoxelHydrologyRegionStep ReadFailure(EVoxelHydrologyClosureRead InRead, FString& OutError) const;
	void FreeScratch();

	FVoxelHydrologyChannelPlan Result;
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
