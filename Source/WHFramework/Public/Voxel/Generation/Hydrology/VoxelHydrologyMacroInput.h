#pragma once

#include "Voxel/Generation/Hydrology/VoxelHydrologyBasinClosure.h"

struct WHFRAMEWORK_API FVoxelHydrologyRegionSample
{
	int32 GroundZ = 0;
	uint32 RainWeight = 0;
	int32 WaterZ = MIN_int32;
	FVoxelStableId WaterId;

	bool IsReceiver() const { return WaterZ != MIN_int32; }
	bool IsValid() const;
	uint64 AppendFingerprint(uint64 InFingerprint) const;
	bool operator==(const FVoxelHydrologyRegionSample& InOther) const = default;
};

// 正式组件与区域摘要共用同一冻结地面、雨量及跨区接收体读取规则。
class WHFRAMEWORK_API FVoxelHydrologyMacroInput
{
public:
    using FTileLookup = TFunctionRef<const FVoxelHydrologyPlan*(const FVoxelHydrologyRegionKey&)>;
    using FRequestTile = TFunctionRef<bool(const FVoxelHydrologyRegionKey&, FString&)>;

    static EVoxelHydrologyClosureRead Read(const FVoxelGenerationSettings& Settings, const FIntPoint& InCell,
        FTileLookup InLookup, FRequestTile InRequestTile, FVoxelHydrologyRegionSample& OutSample,
        FIntPoint& OutWorldPosition, FString& OutError);
    static bool IsRegion(const FVoxelHydrologyRegionKey& InKey, const FVoxelGenerationSettings& InSettings);

private:
    static bool IsCell(const FIntPoint& InCell, int32 InCellSize);
};
