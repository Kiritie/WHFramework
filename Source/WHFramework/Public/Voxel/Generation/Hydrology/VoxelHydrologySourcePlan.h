#pragma once

#include "Voxel/Generation/Hydrology/VoxelHydrologySourceBucket.h"

struct WHFRAMEWORK_API FVoxelHydrologySelectedSource
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	FVoxelStableId Id;
	uint64 Accumulation = 0;
	uint32 Location = 0;
};

struct WHFRAMEWORK_API FVoxelHydrologySourcePlan
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	uint64 Generation = 0;
	uint64 GraphFingerprint = 0;
	uint64 RunoffFingerprint = 0;
	uint64 Fingerprint = 0;
	TArray<FVoxelHydrologySelectedSource> Sources;

	bool HasSourceInRange(uint32 InFirst, uint32 InEndExclusive) const;
	uint64 GetAllocatedBytes() const;
};

// 只从宏观查询选源，剥枝源头沿既有父链接到摘要；不重新展开上游细格。
class WHFRAMEWORK_API FVoxelHydrologySourcePlanBuild
{
public:
	using FReadBucket = TFunctionRef<EVoxelHydrologyClosureRead(const FIntPoint&, const FVoxelHydrologySourceBucket*&, FString&)>;

	FVoxelHydrologySourcePlanBuild();
	~FVoxelHydrologySourcePlanBuild();
	bool Initialize(const FVoxelHydrologyRegionGraph& InGraph, const FVoxelHydrologyNetworkRunoff& InRunoff,
		const FVoxelGenerationSettings& InSettings, FString& OutError);
	EVoxelHydrologyRegionStep Advance(const FVoxelHydrologyRegionGraph& InGraph, const FVoxelHydrologyNetworkRunoff& InRunoff,
		FReadBucket InReadBucket, int32 InWorkBudget, FString& OutError, const TAtomic<bool>* InCancel = nullptr);
	const FVoxelHydrologySourcePlan& GetResult() const;
	FVoxelHydrologySourcePlan TakeResult();
	uint64 GetAllocatedBytes() const;

private:
	enum class EStage : uint8
	{
		None,
		Scan,
		Attach,
		Sort,
		Hash,
		Ready,
		Taken
	};
	void Sift(int32 InRoot, int32 InEnd);
	EVoxelHydrologyRegionStep Failure(FString& OutError) const;

	FVoxelHydrologySourcePlan Result;
	FVoxelHydrologySelectedSource Pending;
	FIntPoint Attachment = FIntPoint::ZeroValue;
	EStage Stage = EStage::None;
	uint64 Threshold = 1;
	int32 Spacing = 1;
	int32 CellCount = 0;
	int32 Cursor = 0;
	int32 WalkSteps = 0;
	int32 SortRoot = INDEX_NONE;
	int32 SortEnd = INDEX_NONE;
};
