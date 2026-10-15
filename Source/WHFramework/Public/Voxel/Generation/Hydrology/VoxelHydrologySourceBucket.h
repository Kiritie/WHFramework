#pragma once

#include "Voxel/Generation/Hydrology/VoxelHydrologyNetworkRunoff.h"

struct WHFRAMEWORK_API FVoxelHydrologySourceBucket
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	FVoxelHydrologySourceCell Winner;
	FVoxelStableId WinnerId;
	uint64 Generation = 0;
	uint64 Fingerprint = 0;
	bool bHasWinner = false;
	uint64 GetAllocatedBytes() const { return 0; }
};

// 每个世界固定间距桶只选一个合法源头，跨区桶仍读取全部实际候选。
class WHFRAMEWORK_API FVoxelHydrologySourceBucketBuild
{
public:
	using FReadCell = TFunctionRef<EVoxelHydrologyClosureRead(const FIntPoint&, FVoxelHydrologySourceCell&, FString&)>;

	FVoxelHydrologySourceBucketBuild();
	~FVoxelHydrologySourceBucketBuild();
	bool Initialize(const FIntPoint& InCoordinate, const FVoxelGenerationSettings& InSettings, uint64 InGeneration, FString& OutError);
	EVoxelHydrologyRegionStep Advance(uint64 InGeneration, FReadCell InReadCell, int32 InWorkBudget,
		FString& OutError, const TAtomic<bool>* InCancel = nullptr);
	const FVoxelHydrologySourceBucket& GetResult() const;
	FVoxelHydrologySourceBucket TakeResult();
	uint64 GetAllocatedBytes() const;

private:
	enum class EStage : uint8
	{
		None,
		Scan,
		Ready,
		Taken
	};
	EVoxelHydrologyRegionStep Failure(FString& OutError) const;

	FVoxelHydrologySourceBucket Result;
	FIntPoint Minimum = FIntPoint::ZeroValue;
	FIntPoint Maximum = FIntPoint::ZeroValue;
	FIntPoint Cursor = FIntPoint::ZeroValue;
	EStage Stage = EStage::None;
	int32 Seed = 0;
	int32 CellSize = 0;
	uint64 Threshold = 1;
};
