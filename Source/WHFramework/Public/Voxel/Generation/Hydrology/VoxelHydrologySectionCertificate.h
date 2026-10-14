#pragma once
#include "Voxel/Generation/Hydrology/VoxelHydrologySharedSearch.h"

struct WHFRAMEWORK_API FVoxelHydrologySectionSample
{
    int32 OriginalGround = 0;
    int32 ExistingWaterZ = MIN_int32;
    FVoxelStableId ExistingBodyId;
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
    int32 MinimumWater = 0;
    int32 Radius = 0;
    int32 X = 0, Y = 0;
    bool bInitialized = false;
    bool bReady = false;
    bool bConflictingWater = false;
    bool bOpenFootprint = false;
};
