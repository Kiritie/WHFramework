#pragma once
#include "CoreMinimal.h"

class WHFRAMEWORK_API FVoxelHydrologyRoutingGrid
{
public:


    bool Initialize(int32 InCoarseCellSize, int32 InMaximumInfluenceRadius,
        const FIntPoint& InWorldMin, const FIntPoint& InWorldMax,
        const FIntPoint& InOrigin, FString& Error);
    int32 GetSpacing() const { return Spacing; }
    int32 GetCoarseMultiplier() const { return Multiplier; }
    bool IsGridNode(const FIntPoint& Position) const;

    bool TryNeighbor(const FIntPoint& GridNode, int32 Face, FIntPoint& Out) const;


    int32 GetReverseCandidates(const FIntPoint& Position, FIntPoint Out[4]) const;
    bool TryMapCoarseCell(const FIntPoint& CoarseCell, FIntPoint& OutGridNode) const;

    bool TryGetCoarseBounds(const FIntPoint& GridNode, FIntPoint& OutMin, FIntPoint& OutMax) const;

    bool TryGetFineBucketBounds(const FIntPoint& GridNode, FIntPoint& OutMin, FIntPoint& OutMax) const;
    bool TryGetBucketOwner(const FIntPoint& FinePosition, FIntPoint& OutGridNode) const;
private:
    bool ToWorld(int64 X, int64 Y, FIntPoint& Out) const;
    int32 CoarseCellSize = 0, Spacing = 0, Multiplier = 0;
    FIntPoint Origin, WorldMin, WorldMax, CoarseMin, CoarseMax;
    int64 IndexMin[2] = {0,0}, IndexMax[2] = {0,0};
    bool bInitialized = false;
};
