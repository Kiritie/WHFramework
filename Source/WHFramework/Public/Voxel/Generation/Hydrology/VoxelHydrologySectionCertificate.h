#pragma once
#include "Voxel/Generation/Hydrology/VoxelHydrologySharedSearch.h"

struct WHFRAMEWORK_API FVoxelHydrologySectionSample
{
    int32 OriginalGround = 0;
    int32 ExistingWaterZ = MIN_int32;
    FVoxelStableId ExistingBodyId;
    bool IsValid() const
    {
        return OriginalGround >= -32768 && OriginalGround <= 32768 &&
            (ExistingWaterZ == MIN_int32 ||
             (ExistingWaterZ >= -32768 && ExistingWaterZ <= 32768 &&
              OriginalGround < ExistingWaterZ && (ExistingBodyId.High != 0 || ExistingBodyId.Low != 0)));
    }
};

struct WHFRAMEWORK_API FVoxelHydrologySectionInterval
{
    int32 LowerWaterZ = MIN_int32;
    int32 UpperWaterZ = MAX_int32;
    int32 FixedWaterZ = MIN_int32;
    FVoxelStableId FixedBodyId;
    FIntPoint FixedConnector = FIntPoint::ZeroValue;
    bool bEligible = false;
};

class WHFRAMEWORK_API FVoxelHydrologySectionCertificate
{
public:
    // 重置前使用同一配方与生成代，已就绪事实保持不可变。
    using FReadSample = TFunctionRef<EVoxelHydrologyClosureRead(
        const FIntPoint&, FVoxelHydrologySectionSample&, FString&)>;
    bool Initialize(const FIntPoint& InPosition, const FVoxelRiverShape& InShape,
        int32 InMinimumGroundZ, const FIntPoint& InWorldMinimum,
        const FIntPoint& InWorldMaximumInclusive, FString& OutError);
    EVoxelHydrologySharedSearchStep Advance(FReadSample Read, int32 WorkBudget,
        FString& OutError, const TAtomic<bool>* Cancel = nullptr);
    bool IsReady() const { return bReady; }
    const FVoxelHydrologySectionInterval& GetResult() const { check(bReady); return Result; }
    void Reset();
private:
    FIntPoint Position, WorldMinimum, WorldMaximum;
    FVoxelRiverShape Shape;
    FVoxelHydrologySectionInterval Result;
    // 续跑状态随证书拥有者重置；仅保留有效就绪采样，等待时不推进游标。
    FVoxelHydrologySectionSample CurrentSample;
    int32 RequiredGround = 0;
    int32 NextNeighbour = 0;
    bool bHasCurrentSample = false;
    int32 MinimumWater = 0;
    int32 Radius = 0;
    int32 X = 0, Y = 0;
    bool bInitialized = false;
    bool bReady = false;
    bool bConflictingWater = false;
    bool bOpenFootprint = false;
};
