#pragma once
#include "Voxel/Generation/Hydrology/VoxelHydrologySharedSearch.h"

class WHFRAMEWORK_API FVoxelHydrologyLakeCertificate
{
public:
    using FReadCell = FVoxelHydrologySharedSearch::FReadCell;
    bool Initialize(const FVoxelHydrologySharedCell& InSeed, const FVoxelStableId& InOceanId,
        const FIntPoint& InMinimum, const FIntPoint& InMaximum, FString& OutError);
    EVoxelHydrologySharedSearchStep Advance(FReadCell InRead, int32 InBudget,
        FString& OutError, const TAtomic<bool>* InCancel = nullptr);
    bool IsReady() const { return bReady; }
    bool IsAccepted() const { check(bReady); return !bLowerContact && !bOpenBoundary; }
    int32 GetWaterZ() const { return WaterZ; }
    FVoxelStableId GetGroupId() const { check(bReady); return GroupId; }
    const TArray<FVoxelHydrologySharedCell>& GetCells() const { check(bReady); return Cells; }
    uint64 GetAllocatedBytes() const;
    void FreeBacking();
private:
    bool Append(const FVoxelHydrologySharedCell& Cell, bool bWet, FString& OutError);
    TArray<FVoxelHydrologySharedCell> Cells;
    TSet<FIntPoint> Seen;
    int32 SeenCapacity = 0;
    int32 Cursor = 0;
    int32 Face = 0;
    int32 WaterZ = MIN_int32;
    FVoxelStableId GroupId, OceanId;
    FIntPoint Minimum, Maximum;
    FVoxelHydrologySharedCell Seed;
    bool bInitialized = false;
    bool bReady = false;
    bool bLowerContact = false;
    bool bOpenBoundary = false;
};
