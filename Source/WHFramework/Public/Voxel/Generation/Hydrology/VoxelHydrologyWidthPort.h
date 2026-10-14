#pragma once
#include "Voxel/Generation/Hydrology/VoxelHydrologySectionCertificate.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyWidthSearch.h"

class WHFRAMEWORK_API FVoxelHydrologyWidthPort
{
public:
    bool Initialize(const FIntPoint& Start, const FIntPoint& End, const FVoxelRiverShape& Shape,
        int32 MinimumGround, const FIntPoint& WorldMin, const FIntPoint& WorldMax, FString& Error);
    bool SetStartEndpointEqualityBlock(int32 HalfLength, const FVoxelHydrologySectionInterval& StartBlock, FString& Error);
    bool SetEndpointEqualityBlocks(int32 HalfLength, const FVoxelHydrologySectionInterval& StartBlock,
        const FVoxelHydrologySectionInterval& EndBlock, FString& Error);
    EVoxelHydrologySharedSearchStep Advance(FVoxelHydrologySectionCertificate::FReadSample Read,
        int32 WorkBudget, FString& Error, const TAtomic<bool>* Cancel = nullptr);
    const FVoxelHydrologyWidthSearchEdge& GetResult() const { check(bReady); return Result; }
    const FVoxelHydrologySectionInterval& GetTerminal() const { check(bReady); return Terminal; }
    const TArray<FVoxelHydrologyWidthNodeInterval>& GetIntervals() const { check(bReady); return Intervals; }
    const TArray<int32>& GetExpansionCounts() const { check(bReady); return ExpansionCounts; }
    uint64 GetAllocatedBytes() const;
    void FreeBacking();
private:
    FIntPoint Position, End, Direction, WorldMin, WorldMax;
    FVoxelRiverShape Shape;
    int32 MinimumGround = 0;
    FVoxelHydrologySectionCertificate Section;
    FVoxelHydrologyWidthComposeState Compose;
    TArray<FVoxelHydrologyWidthNodeInterval> Intervals;
    TArray<int32> ExpansionCounts;
    FVoxelHydrologySectionInterval StartBlock, EndBlock;
    int32 EqualityHalfLength = 0;
    bool bEndEqualityBlock = false;
    FVoxelHydrologyWidthSearchEdge Result;
    FVoxelHydrologySectionInterval Terminal;
    bool bInitialized = false, bSectionStarted = false, bPathComplete = false, bReady = false;
};
