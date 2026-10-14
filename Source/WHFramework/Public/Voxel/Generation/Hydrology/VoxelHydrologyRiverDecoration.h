#pragma once

#include "Voxel/Generation/Hydrology/VoxelHydrologySectionCertificate.h"
#include "Voxel/Generation/Hydrology/VoxelRiverCorridor.h"

struct WHFRAMEWORK_API FVoxelHydrologyRiverDecorationSettings
{
    int32 DesiredWaterHalfWidth = 1;
    int32 DesiredDepth = 1;
    int32 Seed = 0;
    int32 MeanderStrength = 0;
    int32 MeanderFrequency = 96;
    int32 MeanderOctaves = 2;
    int32 MaxMeanderAngle = 35;
    int32 JunctionHalfLength = 1;
    int32 SafeTubeRadius = 1;
    int32 MaxCutFill = 64;
    int32 MinimumGroundZ = -32768;
    FIntPoint WorldMinimum = FIntPoint::ZeroValue;
    FIntPoint WorldMaximum = FIntPoint::ZeroValue;

    int32 WalkableWindowLength = 16;
    int32 WalkableClearanceHalfWidth = 2;
    int32 WalkableLandingLength = 3;
    bool bEnableWalkableMargins = true;
};

namespace VoxelHydrology
{

    WHFRAMEWORK_API bool EvaluateCertifiedRiverSection(int32 InOriginalGround,
        int32 InWaterPlane, int32 InDistanceCells, const FVoxelRiverShape& InShape,
        FVoxelRiverSection& OutSection, FString& OutError);
}

class WHFRAMEWORK_API FVoxelHydrologyRiverDecoration
{
public:
    using FReadSample = FVoxelHydrologySectionCertificate::FReadSample;
    FVoxelHydrologyRiverDecoration() = default;
    FVoxelHydrologyRiverDecoration(const FVoxelHydrologyRiverDecoration&) = delete;
    FVoxelHydrologyRiverDecoration& operator=(const FVoxelHydrologyRiverDecoration&) = delete;
    FVoxelHydrologyRiverDecoration(FVoxelHydrologyRiverDecoration&&) = default;
    FVoxelHydrologyRiverDecoration& operator=(FVoxelHydrologyRiverDecoration&&) = default;
    bool Initialize(const TArray<FVoxelRiverRoutePoint>& InRaw,
        const FVoxelHydrologyRiverDecorationSettings& InSettings, FString& OutError);
    EVoxelHydrologySharedSearchStep Advance(FReadSample Read, int32 WorkBudget,
        FString& OutError, const TAtomic<bool>* Cancel = nullptr);
    bool IsReady() const { return Stage == EStage::Ready; }
    bool UsedBaseFallback() const { return bBaseFallback; }
    bool HasWalkableWindow() const { return IsReady() && !bBaseFallback && WalkBegin != INDEX_NONE; }
    const TArray<FVoxelRiverMeanderPoint>& GetPoints() const { check(IsReady()); return Points; }
    TArray<FVoxelRiverMeanderPoint> TakePoints();
    uint64 GetAllocatedBytes() const;
    void FreeBacking();

private:
    enum class EStage : uint8
{
    None,
    Validate,
    Backward,
    Build,
    Angles,
    Raster,
    Walk,
    Fallback,
    Ready
};
    FVoxelRiverMeanderPoint BasePoint(int32 Index) const;
    FVoxelRiverMeanderPoint DecoratedPoint(int32 Index) const;
    bool IsBaseContact(const FIntPoint& Cell, int32 WaterZ) const;
    bool IsBaseWet(const FIntPoint& Cell, int32 WaterZ) const;
    bool InWorld(const FIntPoint& Cell) const;
    void RejectCandidate();
    void NextRasterCell();
    EVoxelHydrologySharedSearchStep ReadAt(FReadSample Read, const FIntPoint& Cell,
        FVoxelHydrologySectionSample& Out, FString& Error) const;
    bool CheckSample(const FVoxelHydrologySectionSample& Sample, FString& Error) const;

    const TArray<FVoxelRiverRoutePoint>* Raw = nullptr;
    FVoxelHydrologyRiverDecorationSettings Settings;
    TArray<FVoxelRiverMeanderPoint> Points;
    TArray<int32> AnchorDistance;
    EStage Stage = EStage::None;
    FIntPoint Direction = FIntPoint::ZeroValue;
    int32 Cursor = 0, LastAnchor = 0, ControlStep = 8, Protection = 0;
    int32 DesiredBed = 1, DesiredWater = 1, DesiredDepth = 1, MaximumRadius = 1;
    int32 WalkWater = 1, WalkRadius = 1;
    int32 RunStart = 0, LongestStart = INDEX_NONE, LongestEnd = INDEX_NONE;
    int32 WalkBegin = INDEX_NONE, WalkEnd = INDEX_NONE;
    int32 RasterSegment = 1, RasterFace = -1, RasterBestDistance = MAX_int32;
    FIntPoint RasterMin, RasterMax, RasterCell;
    bool bRasterStarted = false;
    FVoxelRiverProjection RasterProjection;
    FVoxelHydrologySectionSample RasterSample;
    FVoxelRiverSection RasterSection;
    int32 WalkAlong = 0, WalkSide = -1, WalkLateral = 0, WalkLane = 0;
    int32 WalkLastOldHeight = 0;
    int32 WalkPrevious[5] = {0,0,0,0,0};
    int32 WalkScanSegment = 1, WalkBestDistance = MAX_int32;
    FVoxelRiverProjection WalkProjection;
    bool bBaseFallback = false;
};
