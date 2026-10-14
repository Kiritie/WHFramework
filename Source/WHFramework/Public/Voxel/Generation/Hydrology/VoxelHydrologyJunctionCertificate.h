#pragma once
#include "Voxel/Generation/Hydrology/VoxelHydrologySectionCertificate.h"

class WHFRAMEWORK_API FVoxelHydrologyJunctionCertificate
{
public:
    bool Initialize(const FIntPoint& Center, const FVoxelRiverShape& Shape, int32 MinimumGround,
        const FIntPoint& WorldMin, const FIntPoint& WorldMax, FString& Error);
    EVoxelHydrologySharedSearchStep Advance(FVoxelHydrologySectionCertificate::FReadSample Read,
        int32 WorkBudget, FString& Error, const TAtomic<bool>* Cancel = nullptr);
    const FVoxelHydrologySectionInterval& GetResult() const { check(bReady); return Result; }
    FIntPoint GetConnectorEnd() const { check(bReady); return ConnectorEnd; }
    int32 GetHalfLength() const { return HalfLength; }
    void Reset();
private:
    FIntPoint Center, WorldMin, WorldMax, ConnectorEnd;
    FVoxelRiverShape Shape;
    FVoxelHydrologySectionCertificate Section;
    FVoxelHydrologySectionInterval Result;
    int32 MinimumGround = 0, HalfLength = 0, Index = 0;
    bool bInitialized = false, bSectionStarted = false, bIneligible = false, bReady = false;
};
