#pragma once

#include "CoreMinimal.h"
#include "Voxel/Authoring/VoxelViewProfile.h"

struct WHFRAMEWORK_API FVoxelPublishGroupKey
{
	uint8 Representation = 0;
	FIntVector Coordinate = FIntVector::ZeroValue;
	uint8 Level = 0;

	FVoxelPublishGroupKey() = default;
	FVoxelPublishGroupKey(uint8 InRepresentation, FIntVector InCoordinate, uint8 InLevel)
		: Representation(InRepresentation), Coordinate(InCoordinate), Level(InLevel)
	{
	}

	FVoxelPublishGroupKey(uint8 InRepresentation, FIntPoint InCoordinate, uint8 InLevel)
		: FVoxelPublishGroupKey(InRepresentation, FIntVector(InCoordinate.X, InCoordinate.Y, 0), InLevel)
	{
	}

	bool operator==(const FVoxelPublishGroupKey& InOther) const = default;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelPublishGroupKey& InKey)
{
	return HashCombineFast(
		HashCombineFast(::GetTypeHash(InKey.Representation), GetTypeHash(InKey.Coordinate)),
		::GetTypeHash(InKey.Level));
}

struct WHFRAMEWORK_API FVoxelPublishFootprint
{
	FVoxelPublishGroupKey Key;
	FBox Bounds = FBox(ForceInit);
	bool bTransition = false;
	bool bIndependentInitial = false;
};

// Pure planning state. Capture one footprint at a time, then advance bounded
// interval queries; only a completed plan can replace publication groups.
class WHFRAMEWORK_API FVoxelPublishGroupContinuation
{
public:
	bool Add(const FVoxelPublishFootprint& InFootprint);
	bool Advance(int32 InMaximumOperations, double InDeadlineSeconds = MAX_dbl);
	const TArray<int32>& GetGroupIndices() const;
	uint64 GetComparedFootprints() const;

private:
	struct FInterval
	{
		int32 Footprint = INDEX_NONE;
		int32 MinimumFootprint = INDEX_NONE;
		int32 Left = INDEX_NONE;
		int32 Right = INDEX_NONE;
		uint32 Priority = 0;
		double MaximumEnd = -MAX_dbl;
		double MinimumY = MAX_dbl;
		double MaximumY = -MAX_dbl;
	};
	int32 FindRoot(int32 InIndex);
	int32 InsertInterval(int32 InRoot, int32 InNode);
	void RefreshInterval(int32 InNode);
	TArray<FVoxelPublishFootprint> Footprints;
	TArray<FInterval> Intervals;
	TArray<int32> Parents;
	TArray<int32> GroupIndices;
	TArray<int32> QueryStack;
	TMap<FVoxelPublishGroupKey, int32> Keys;
	TMap<FVoxelPublishGroupKey, int32> Owners;
	TMap<int32, int32> Ordinals;
	int32 IntervalRoot = INDEX_NONE;
	int32 Phase = 0;
	int32 Cursor = 0;
	bool bQueryStarted = false;
	uint64 ComparedFootprints = 0;
};

class WHFRAMEWORK_API FVoxelPublishGroupPlanner
{
public:
	static FBox OwnershipBounds(
		const FVoxelPublishGroupKey& InKey,
		double InBlockSize, const FVoxelLodTopologySettings& InLod = FVoxelLodTopologySettings());

	static FBox PublicationBounds(
		const FVoxelPublishGroupKey& InKey,
		double InBlockSize,
		const FBox& InPresentedBounds,
		const FBox& InDesiredBounds,
		bool bInTransition, const FVoxelLodTopologySettings& InLod = FVoxelLodTopologySettings());

	static void Build(
		TConstArrayView<FVoxelPublishFootprint> InFootprints,
		TArray<int32>& OutGroupIndices);
};

// A snapshot-owned spatial query. A continuation visits one node or record per
// operation; its callers retain the publication generations backing its ordinals.
class WHFRAMEWORK_API FVoxelPublishBoundsIndex
{
private:
	struct FKey
	{
		FIntPoint Coordinate = FIntPoint::ZeroValue;
		uint8 Level = 0;
		bool operator==(const FKey&) const = default;
		friend uint32 GetTypeHash(const FKey& Key)
		{
			return HashCombineFast(GetTypeHash(Key.Coordinate), GetTypeHash(Key.Level));
		}
	};
	struct FNode
	{
		TSet<int32> Own;
		uint64 MinimumSerial = MAX_uint64;
	};
	struct FRecord
	{
		FBox Bounds = FBox(ForceInit);
		uint64 Serial = 0;
	};

public:
	struct FQuery
	{
		FBox Bounds = FBox(ForceInit);
		uint64 MaximumSerial = MAX_uint64;
		TArray<FKey> Stack;
		TSet<int32> Seen;
		FKey Current;
		int32 OwnCursor = 0;
		bool bHasCurrent = false;
	};
	void Add(int32 InOrdinal, const FBox& InBounds, uint64 InSerial = 0);
	void BeginQuery(const FBox& InBounds, FQuery& OutQuery, uint64 InMaximumSerial = MAX_uint64) const;
	// true means completed; otherwise OutOrdinal may contain one local candidate.
	bool AdvanceQuery(FQuery& InOutQuery, int32& OutOrdinal) const;

private:
	TMap<FKey, FNode> Nodes;
	TMap<int32, FRecord> Records;
	uint8 RootLevel = 0;
};
